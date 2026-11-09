#include "internal.h"

#include <stdlib.h>
#include <string.h>

static const char* get_event_name(TransitionNode* transition_node) {
  const char* name = NULL;
  Expression* event_expression = transition_node->event;
  if(event_expression->type == EXPRESSION_IDENTIFIER) {
    IdentifierExpression* idexpr = (IdentifierExpression*)event_expression;
    name = idexpr->name;
  } else if(event_expression->type == EXPRESSION_ON) {
    OnExpression* onexpr = (OnExpression*)event_expression;
    name = onexpr->name;
  }
  return name;
}

static void link_event_transition(Node* parent_node, TransitionNode* transition_node) {
  const char* event_name = get_event_name(transition_node);

  TransitionNode* child_transition_node = NULL;
  switch(parent_node->type) {
    case NODE_STATE: {
      StateNode* state_node = (StateNode*)parent_node;
      child_transition_node = state_node->event_transition;
      if(child_transition_node == NULL) {
        state_node->event_transition = transition_node;
        return;
      }
      break;
    }
    case NODE_INVOKE: {
      InvokeNode* invoke_node = (InvokeNode*)parent_node;
      child_transition_node = invoke_node->event_transition;
      if(child_transition_node == NULL) {
        invoke_node->event_transition = transition_node;
        return;
      }
      break;
    }
    default: {
      break;
    }
  }

  if(child_transition_node != NULL) {
    // Transitions for the same event are chained through link, starting
    // from the first one. Only link into that first chain: the later
    // transitions are in it already, and linking from them too would make
    // the chain loop back on itself (with three or more, it did, and the
    // printer never finished).
    bool linked_already = false;
    do {
      const char* name = get_event_name(child_transition_node);
      if(!linked_already && name != NULL && event_name != NULL &&
        strcmp(name, event_name) == 0) {
        TransitionNode* linked = child_transition_node;
        while(linked->link != NULL) {
          linked = linked->link;
        }
        linked->link = transition_node;
        linked_already = true;
      }

      if(child_transition_node->next == NULL) {
        child_transition_node->next = transition_node;
        break;
      } else {
        child_transition_node = child_transition_node->next;
      }
    } while(true);
  }
}

// Add transition_node to the end of the list that starts at *list.
static void append_transition(TransitionNode** list, TransitionNode* transition_node) {
  while(*list != NULL) {
    list = &(*list)->next;
  }
  *list = transition_node;
}

static ParseStatus consume_on_args(Parser* parser, void* expr, TokenType token, const char* arg, size_t index) {
  (void)parser;
  (void)token;
  (void)index;
  OnExpression* on_expression = (OnExpression*)expr;
  on_expression->name = arg;
  return PARSE_OK;
}

static ParseStatus consume_on(Parser* parser, TransitionNode* transition_node) {
  ParseStatus status = PARSE_OK;

  OnExpression* expression = expression_create_on(parser->arena);
  if(parse_merge(&status, parser_consume_call(parser, "on", expression, &consume_on_args)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }
  transition_node->event = (Expression*)expression;
  if(parse_merge(&status, parser_expect_call_argument(parser, "on", expression->name, status)) == PARSE_FATAL) {
    return PARSE_FATAL;
  }

  return status;
}

static ParseStatus consume_member_expression(Parser* parser, TransitionNode* transition_node) {
  ParseStatus status = PARSE_OK;

  MemberExpression* top_member_expression = expression_create_member(parser->arena);
  top_member_expression->owner = parser_take_word(parser);
  parser_next(parser);
  TokenType token = parser_next_token(parser);

  if(token != TOKEN_IDENTIFIER) {
    parser_error_unexpected_identifier(parser);
    status = PARSE_ERROR;
    goto exit;
  } else {
    IdentifierExpression* prop = expression_create_identifier(parser->arena);
    prop->name = parser_take_word(parser);
    top_member_expression->property = (Expression*)prop;
    transition_node->dest = (Expression*)top_member_expression;
  }

  exit: {
    return status;
  }
}

/* Public API */
ParseStatus parser_consume_transition(Parser* parser) {
  ParseStatus status = PARSE_OK;
  TransitionNode* transition_node = node_create_transition(parser->arena);

  Node* current_node = parser->node;
  if(current_node == NULL) {
    parser_error(parser, "Unexpected parent node to a transition.");
    return PARSE_FATAL;
  }
  NodeType current_node_type = current_node->type;

  const char* event = parser_take_word(parser);

  // Always transition
  if(event == NULL) {
    if(current_node->type != NODE_STATE) {
      parser_error_back(parser, parser->token_len, "An immediate transitions must be a child of state");
      status = PARSE_ERROR;
    } else {
      transition_node->type = TRANSITION_IMMEDIATE;
      append_transition(&((StateNode*)current_node)->immediate_transition, transition_node);
    }

    // Currently in a call, so rewind to back out.
    parser_rewind(parser, 2);
  } else {
    Keyword key = keyword_lookup(event);

    switch(key) {
      case KEYWORD_DELAY: {
        if(current_node->type != NODE_STATE) {
          parser_error_back(parser, parser->token_len, "delay must be a child of a state");
          status = PARSE_ERROR;
        } else {
          if(parse_merge(&status, parser_consume_inline_delay(parser, transition_node, (StateNode*)current_node)) == PARSE_FATAL) {
            return PARSE_FATAL;
          }
          append_transition(&((StateNode*)current_node)->delay_transition, transition_node);
        }

        break;
      }
      case KEYWORD_ON: {
        if(parse_merge(&status, consume_on(parser, transition_node)) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        link_event_transition(current_node, transition_node);
        break;
      }
      default: {
        IdentifierExpression* identifier_expression = expression_create_identifier(parser->arena);
        identifier_expression->name = event;
        transition_node->event = (Expression*)identifier_expression;
        link_event_transition(current_node, transition_node);
        break;
      }
    }
  }

  Node* transition_node_node = (Node*)transition_node;

  switch(current_node_type) {
    case NODE_STATE: {
      node_append_child(current_node, transition_node_node);
      parser->parent_node = current_node;
      break;
    }
    case NODE_INVOKE: {
      node_append_child(current_node, transition_node_node);
      parser->parent_node = current_node;
      break;
    }
    default: {
      parser_error(parser, "Unexpected parent node to a transition.");
      return PARSE_FATAL;
    }
  }

  parser->node = transition_node_node;

  bool block_transition = false;
  const char* identifier = NULL;
  while(true) {
    TokenType token = parser_next_token(parser);

    if(token == TOKEN_EOL) {
      if(block_transition) {
        token = parser_next_token(parser);

        while(token == TOKEN_EOL) {
          token = parser_next_token(parser);
        }

        if(token != TOKEN_END_BLOCK) {
          parser_error(parser, "Block transition expects destination state.");
          status = PARSE_FATAL;
          goto end;
        }

        break;
      }
      // End of the transition
      break;
    }

    if(token != TOKEN_CALL) {
      parser_error_back(parser, parser->token_len, "Expected a destination state for this event.");
      status = PARSE_FATAL;
      goto end;
    }

    token = parser_next_token(parser);

    while(block_transition && token == TOKEN_EOL) {
      token = parser_next_token(parser);
    }

    if(token == TOKEN_BEGIN_BLOCK && !block_transition) {
      block_transition = true;
      token = parser_next_token(parser);

      while(token == TOKEN_EOL) {
        token = parser_next_token(parser);
      }
    }

    switch(token) {
      case TOKEN_IDENTIFIER: {
        identifier = parser_take_word(parser);
        Keyword key = keyword_lookup(identifier);
        switch(key) {
          // Inline guard
          case KEYWORD_GUARD: {
            if(parse_merge(&status, parser_consume_inline_guard(parser, transition_node)) == PARSE_FATAL) {
              return PARSE_FATAL;
            }
            break;
          }
          case KEYWORD_ASSIGN: {
            if(parse_merge(&status, parser_consume_inline_assign(parser, transition_node_node)) == PARSE_FATAL) {
              return PARSE_FATAL;
            }
            break;
          }
          case KEYWORD_ACTION: {
            if(parse_merge(&status, parser_consume_inline_action(parser, transition_node_node)) == PARSE_FATAL) {
              return PARSE_FATAL;
            }
            break;
          }
          case KEYWORD_SEND: {
            if(parse_merge(&status, parser_consume_inline_send(parser, transition_node_node)) == PARSE_FATAL) {
              return PARSE_FATAL;
            }
            break;
          }
          default: {
            if(parser_has_guard_name(parser, identifier)) {
              node_transition_add_guard(parser->arena, transition_node, identifier);
            } else if(parser_has_action_name(parser, identifier)) {
              node_transition_add_action(parser->arena, transition_node, identifier);
            } else {
              IdentifierExpression* identifier_expression = expression_create_identifier(parser->arena);
              identifier_expression->name = identifier;
              transition_node->dest = (Expression*)identifier_expression;
            }
            break;
          }
        }
        break;
      }
      case TOKEN_MEMBER_EXPRESSION: {
        if(parse_merge(&status, consume_member_expression(parser, transition_node)) == PARSE_FATAL) {
          return PARSE_FATAL;
        }
        break;
      }
      default: {
        parser_error_unexpected_identifier(parser);
        status = PARSE_FATAL;
        goto end;
      }
    }
  }

  // A transition needs at least one of a destination, a guard or an
  // action. Without any (an event name alone on a line, or `delay(1s)`)
  // the printer had no destination to print and crashed.
  if(status == PARSE_OK && transition_node->dest == NULL &&
    transition_node->guard == NULL && transition_node->action == NULL) {
    parser_error_back(parser, parser->token_len, "Expected a destination state for this event.");
    status = PARSE_ERROR;
  }

  end: {
    parser_leave_node(parser);
    return status;
  }
}