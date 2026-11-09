#include "internal.h"

#include <string.h>

static int consume_use_specifiers(ImportNode* import_node, Parser* parser) {
  int err = 0;
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

          parser_error(parser, (Node*)import_node, "Import aliases are not currently supported.");
          return 2;
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
        parser_error_unexpected_identifier(parser, (Node*)import_node);
        goto end;
      }
    }
  }

  end: {
    return err;
  }
}

int parser_consume_use(Parser* parser) {
  int err = 0;
  Node *current_node = parser->node;

  if(current_node != NULL) {
    parser_error(parser, current_node, "Import statement must be at the top of the file.");
    // Return rather than go to end: no node has been pushed yet, so
    // parser_leave_node would pop the enclosing machine, and the next state
    // would find no parent.
    return 1;
  }

  ImportNode *import_node = node_create_import(parser->arena);
  Node *node = (Node*)import_node;
  parser_node_start_pos(parser, node, 4);
  parser_enter_node(parser, node);

  TokenType token;

  token = parser_next_token(parser);

  // use './util'
  if(token != TOKEN_STRING) {
    parser_error(parser, current_node, "Expected a string.");
    err = 1;
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
      };
      case TOKEN_BEGIN_BLOCK: {
        _check(consume_use_specifiers(import_node, parser));
        consumed_specifiers = true;

        break;
      }
      case TOKEN_IDENTIFIER: {
        parser_error_unexpected_identifier(parser, node);
        err = 1;
        goto end;
      }
      case TOKEN_UNKNOWN: {
        parser_error_unexpected_identifier(parser, node);
        err = 2;
        goto end;
      }
      default: {
        parser_error_unexpected_identifier(parser, node);
        err = 2;
        goto end;
      }
    }
  }

  end: {
    parser_leave_node(parser);
    return err;
  }
}