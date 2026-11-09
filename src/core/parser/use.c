#include "internal.h"

#include <string.h>

static ParseStatus consume_use_specifiers(ImportNode* import_node, Parser* parser) {
  ParseStatus status = PARSE_OK;
  TokenType token;

  // The specifier read but not yet added. Only add one once: `{ a,, b }`
  // used to append a twice, which made the list loop, and `{ }` appended
  // an uninitialised pointer.
  ImportSpecifierNode *specifier = NULL;
  while(true) {
    token = parser_next_token(parser);
    
    switch(token) {
      case TOKEN_IDENTIFIER: {
        char* identifier = parser_take_word(parser);
        if(strcmp(identifier, "as") == 0) {

          parser_error(parser, "Import aliases are not currently supported.");
          return PARSE_FATAL;
        }

        specifier = node_create_import_specifier(parser->arena, identifier);
        break;
      }
      case TOKEN_END_BLOCK: {
        if(specifier != NULL) {
          node_append_child((Node*)import_node, (Node*)specifier);
        }
        goto end;
      }
      case TOKEN_EOL: {
        break;
      }
      case TOKEN_COMMA: {
        if(specifier != NULL) {
          node_append_child((Node*)import_node, (Node*)specifier);
          specifier = NULL;
        }
        break;
      }
      default: {
        parser_error_unexpected_identifier(parser);
        goto end;
      }
    }
  }

  end: {
    return status;
  }
}

ParseStatus parser_consume_use(Parser* parser) {
  ParseStatus status = PARSE_OK;
  Node *current_node = parser->node;

  if(current_node != NULL) {
    parser_error(parser, "Import statement must be at the top of the file.");
    // Return rather than go to end: no node has been pushed yet, so
    // parser_leave_node would pop the enclosing machine, and the next state
    // would find no parent.
    return PARSE_ERROR;
  }

  ImportNode *import_node = node_create_import(parser->arena);
  Node *node = (Node*)import_node;
  parser_node_start_pos(parser, node, 4);
  parser_enter_node(parser, node);

  TokenType token;

  token = parser_next_token(parser);

  // use './util'
  if(token != TOKEN_STRING) {
    parser_error(parser, "Expected a string.");
    status = PARSE_ERROR;
    goto end;
  }
  import_node->from = parser_take_word(parser);

  // Start grabbing specififers
  bool consumed_loc = false;
  bool consumed_specifiers = false;
  bool consumed_from = false;
  loop: while(true) {
    token = parser_next_token(parser);

    switch(token) {
      case TOKEN_EOL: {
        if(consumed_specifiers) {
          goto end;
        }
        // Known bug: a newline before the specifiers falls through and is
        // parsed as if it were the {.
      }
      // fall through
      case TOKEN_BEGIN_BLOCK: {
        if(parse_merge(&status, consume_use_specifiers(import_node, parser)) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        consumed_specifiers = true;

        break;
      }
      case TOKEN_IDENTIFIER: {
        parser_error_unexpected_identifier(parser);
        status = PARSE_ERROR;
        goto end;
      }
      case TOKEN_UNKNOWN: {
        parser_error_unexpected_identifier(parser);
        status = PARSE_FATAL;
        goto end;
      }
      default: {
        parser_error_unexpected_identifier(parser);
        status = PARSE_FATAL;
        goto end;
      }
    }
  }

  end: {
    parser_leave_node(parser);
    return status;
  }
}