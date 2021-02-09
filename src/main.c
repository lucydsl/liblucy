#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "node.c"
#include "state.c"
#include "identifier.c"
#include "scope.c"

int is_binding(char* word)
{
  return strcmp(word, "state") == 0;
}

int is_newline(char c)
{
  return c == '\n';
}

int is_whitespace(char c)
{
  return c == ' ';
}

char peek(char * source)
{
  source++;
  char c = *source;
  source--;
  return c;
}

Node parse(char * source) {
  State* state = state_new_state();

  //size_t i = 0;
  while(*source != '\0') {
    char c = *source;

    if(is_newline(c) == 1) {
      // Possibly end some types of expressions
      if(state->node != NULL) {
        Node * node = state->node;

        if(node->type == NODE_TRANSITION_TYPE) {
          TransitionNode* transition_node = (TransitionNode*)node;
          transition_node->dest = state->word;

          printf("Transition node info, event: %s, word: %s\n", transition_node->event, transition_node->dest);

          state->node = state->parent_node;
          state_reset_word(state);
        }
      }

      state_advance_line(state);

      source++;
      continue;
    }

    if(is_whitespace(c) == 1) {
      if(state_in_word(state) == 1) {
        char* word = state->word;

        if(is_binding(word) == 1) {
          if(strcmp(word, "state") == 0) {
            Node* state_node = node_create_type(NODE_STATE_TYPE);
            state->parent_node = state->node;
            state->node = state_node;
          }
        }

        // TODO is this right?
        state_reset_word(state);      
      }

      state_advance_column(state);
      
      source++;
      continue;
    }

    if(c == '{') {
      Scope* scope = scope_create_scope(state->scope);
      scope->node = state->node;
      state->scope = scope;

      if(state->node != NULL) {
        Node * node = state->node;

        if(node->type == NODE_STATE_TYPE) {
          node_set_name(node, state->word);
          state_reset_word(state);
        }        
      }

      source++;
      continue;
    }

    if(c == '}') {
      // TODO end scope
    }

    if(c == '=') {
      if(peek(source) == '>') {
        // TODO handle guards/actions inside of a transition

        TransitionNode* transition_node = node_create_transition();
        printf("WTF is the word %s\n", state->word);
        strcpy(transition_node->event, state->word);
        //transition_node->event = state->word;
        state_reset_word(state);
        printf("Creating transition, event is %s\n", transition_node->event);

        // Should be a state node, should we check here? TODO
        Node* node = (Node*)transition_node;
        node->parent = state->node;

        state->parent_node = state->node;
        state->node = (Node*)transition_node;
      } else {
        printf("This is an assignment\n");
      }
    }

    if(is_valid_identifier_char(c) == 1) {      
      char char_string[2] = {0};
      char buffer[100] = {""};
      
      do {
        char_string[0] = c;
        strcat(buffer, char_string);
        
        state_advance_column(state);
        source++;
        c = *source;
      } while(is_valid_identifier_char(c) == 1);

      state_set_word(state, buffer);

      continue;
    }

    source++;
  }

  Node node;
  return node;
}

char* compile(char * source) {
  parse(source);

  return "testing";
}

int main() {
  identifier_init();
}