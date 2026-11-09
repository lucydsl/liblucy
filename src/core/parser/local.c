#include "internal.h"

ParseStatus parser_consume_local(Parser* parser, LocalKind key) {
  ParseStatus status = PARSE_OK;

  LocalNode* local_node = node_create_local(parser->arena);
  Node* node = (Node*)local_node;
  local_node->key = key;

  Node* parent_node = parser->node;
  if(parent_node == NULL || parent_node->type != NODE_STATE) {
    parser_error_backf(parser, parser->token_len, "Unexpected parent of %s", local_name(key));
    status = PARSE_ERROR;
  }

  parser_enter_node(parser, node);

  // Only a state has entry and exit slots; writing them into another
  // kind of node would write past its end.
  if(parent_node != NULL && parent_node->type == NODE_STATE) {
    StateNode* parent_state_node = (StateNode*)parent_node;
    if(key == LOCAL_ENTRY) {
      parent_state_node->entry = local_node;
    } else {
      parent_state_node->exit = local_node;
    }
  }

  TokenType token;
  while(true) {
    token = parser_next_token(parser);

    if(token == TOKEN_EOL) {
      break;
    }

    if(token != TOKEN_CALL) {
      parser_error_back(parser, parser->token_len, "Expected a => here.");
      status = PARSE_ERROR;
    }

    token = parser_next_token(parser);

    if(token != TOKEN_IDENTIFIER) {
      parser_error_back(parser, parser->token_len, "Expected an action.");
      status = PARSE_FATAL;
      goto end;
    }

    const char* identifier = parser_take_word(parser);
    // Not "key": that would hide the @entry/@exit key, which the guard
    // error below needs (it printed local_name(KEYWORD_GUARD), a NULL).
    Keyword keyword = keyword_lookup(identifier);
    switch(keyword) {
      case KEYWORD_ACTION: {
        if(parse_merge(&status, parser_consume_inline_action(parser, node)) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        break;
      }
      case KEYWORD_ASSIGN: {
        if(parse_merge(&status, parser_consume_inline_assign(parser, node)) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        break;
      }
      case KEYWORD_SEND: {
        if(parse_merge(&status, parser_consume_inline_send(parser, node)) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        break;
      }
      case KEYWORD_GUARD: {
        status = PARSE_ERROR;
        parser_error_backf(parser, parser->token_len, "Guards are not allowed in %s", local_name(key));
        break;
      }
      // Not a keyword
      case KEYWORD_NONE: {
        if(parser_has_action_name(parser, identifier)) {
          TransitionAction* action = node_create_transition_action(parser->arena);
          action->name = identifier;
          node_local_add_action(local_node, action);
        } else {
          status = PARSE_ERROR;
          parser_error_backf(parser, parser->token_len, "Unknown action [%s]. Did you mean to declare this action at the top of the machine?", identifier);
        }
        break;
      }
      default: {
        parser_error_back(parser, parser->token_len, "Unexpected keyword in action assignment.");
        status = PARSE_ERROR;
        break;
      }
    }
  }

  // `@entry` on its own has no actions, and the printer crashed on the
  // empty list.
  if(status == PARSE_OK && local_node->action == NULL) {
    parser_error_back(parser, parser->token_len, "Expected a => here.");
    status = PARSE_ERROR;
  }

end: {
  parser_leave_node(parser);
  return status;
}
}
