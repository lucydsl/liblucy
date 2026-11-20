#include "internal.h"

#include <string.h>

/*
 * `input name` declares a key of the context that whoever creates the
 * machine has to supply, and `input name(Type)` one with a type. A machine
 * that spawns it gives the inputs in a block after the spawn; anything
 * else passes them in the context option. The type is a single name, as
 * an event's is. The declarations change nothing in the machine's own
 * JavaScript; they type the .d.ts and let the printer check the spawns.
 */

// Why an input can't be declared where the parser is, or NULL if it can:
// only in the block of a named machine at the top level of the file.
static const char* misplaced(const Parser* parser) {
  const Node* node = parser->node;
  if(node == NULL || node->type != NODE_MACHINE || !((const MachineNode*)node)->has_name) {
    return "Inputs are declared inside a machine { } block.";
  }
  if(parser->machine_depth > 1) {
    return "Inputs are declared on the outermost machine, not on a machine nested in a state.";
  }
  return NULL;
}

static bool is_declared(const Node* machine_node, const char* name) {
  for(const Node* child = machine_node->child; child != NULL; child = child->next) {
    if(child->type == NODE_INPUT && strcmp(((const InputNode*)child)->name, name) == 0) {
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

ParseStatus parser_consume_input(Parser* parser) {
  ParseStatus status = PARSE_OK;

  const char* placement = misplaced(parser);
  if(placement != NULL) {
    parser_error_back(parser, parser->token_len, placement);
    status = PARSE_ERROR;
  }

  TokenType token = parser_next_token(parser);
  if(token != TOKEN_IDENTIFIER) {
    parser_error_back(parser, parser->token_len, "Inputs must be given a name.");
    skip_declaration(parser, token);
    return PARSE_ERROR;
  }
  const char* name = parser_take_word(parser);
  if(status == PARSE_OK && is_declared(parser->node, name)) {
    parser_error_backf(parser, parser->token_len, "The input %s is already declared.", name);
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
        "An input's type is a single name, as in input %s(Type). Give any other type a name in TypeScript and import it with use.", name);
      skip_declaration(parser, token);
      return PARSE_ERROR;
    }
    token = parser_next_token(parser);
  }

  if(token == TOKEN_END_BLOCK) {
    parser_rewind(parser, parser->token_len);
  } else if(token != TOKEN_EOL && token != TOKEN_EOF) {
    parser_error_backf(parser, parser->token_len,
      "Expected the end of the line after an input. Its type goes in parentheses, as in input %s(Type).", name);
    skip_declaration(parser, token);
    return PARSE_ERROR;
  }

  if(status == PARSE_OK) {
    InputNode* input_node = node_create_input(parser->arena);
    input_node->name = name;
    input_node->type = type;
    node_append_child(parser->node, (Node*)input_node);
  }
  return status;
}
