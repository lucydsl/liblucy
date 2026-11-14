#include "internal.h"

#include <string.h>

/*
 * `event name` declares an event of the machine it is in, and
 * `event name(Type)` one whose data has a type. The type is only ever a
 * name: one imported with `use`, or one TypeScript already knows (string,
 * number, MouseEvent). The declarations change nothing in the JavaScript;
 * they type the .d.ts and let the printer check the machine's events.
 */

// Why an event can't be declared where the parser is, or NULL if it can:
// only in the block of a named machine at the top level of the file.
static const char* misplaced(const Parser* parser) {
  const Node* node = parser->node;
  if(node == NULL || node->type != NODE_MACHINE || !((const MachineNode*)node)->has_name) {
    return "Events are declared inside a machine { } block.";
  }
  if(parser->machine_depth > 1) {
    return "Events are declared on the outermost machine, not on a machine nested in a state.";
  }
  return NULL;
}

static bool is_declared(const Node* machine_node, const char* name) {
  for(const Node* child = machine_node->child; child != NULL; child = child->next) {
    if(child->type == NODE_EVENT && strcmp(((const EventNode*)child)->name, name) == 0) {
      return true;
    }
  }
  return false;
}

// Leave the rest of a declaration with an error in it, token being the
// last one read. A } is left for the machine to read.
static void skip_declaration(Parser* parser, TokenType token) {
  while(token != TOKEN_EOL && token != TOKEN_EOF) {
    if(token == TOKEN_END_BLOCK) {
      parser_rewind(parser, parser->token_len);
      return;
    }
    token = parser_next_token(parser);
  }
}

ParseStatus parser_consume_event(Parser* parser) {
  ParseStatus status = PARSE_OK;

  const char* placement = misplaced(parser);
  if(placement != NULL) {
    parser_error_back(parser, parser->token_len, placement);
    status = PARSE_ERROR;
  }

  TokenType token = parser_next_token(parser);
  if(token != TOKEN_IDENTIFIER) {
    parser_error_back(parser, parser->token_len, "Events must be given a name.");
    skip_declaration(parser, token);
    return PARSE_ERROR;
  }
  const char* name = parser_take_word(parser);
  if(status == PARSE_OK && is_declared(parser->node, name)) {
    parser_error_backf(parser, parser->token_len, "The event %s is already declared.", name);
    status = PARSE_ERROR;
  }

  const char* type = NULL;
  token = parser_next_token(parser);
  if(token == TOKEN_BEGIN_CALL) {
    token = parser_next_token(parser);
    if(token == TOKEN_IDENTIFIER) {
      type = parser_take_word(parser);
      token = parser_next_token(parser);
    }
    if(type == NULL || token != TOKEN_END_CALL) {
      parser_error_backf(parser, parser->token_len,
        "An event's type is a single name, as in event %s(Type). Give any other type a name in TypeScript and import it with use.", name);
      skip_declaration(parser, token);
      return PARSE_ERROR;
    }
    token = parser_next_token(parser);
  }

  if(token == TOKEN_END_BLOCK) {
    parser_rewind(parser, parser->token_len);
  } else if(token != TOKEN_EOL && token != TOKEN_EOF) {
    parser_error_backf(parser, parser->token_len,
      "Expected the end of the line after an event. Its type goes in parentheses, as in event %s(Type).", name);
    skip_declaration(parser, token);
    return PARSE_ERROR;
  }

  if(status == PARSE_OK) {
    EventNode* event_node = node_create_event(parser->arena);
    event_node->name = name;
    event_node->type = type;
    node_append_child(parser->node, (Node*)event_node);
  }
  return status;
}
