#include "parser.h"

#include "internal.h"

#include <string.h>

void parser_init(Parser* parser, Arena* arena, Program* program, const char* source, const char* filename, DiagnosticList* diagnostics) {
  // Zero everything first: the parser reads modifier and the word
  // fields before it ever sets them.
  memset(parser, 0, sizeof(*parser));
  parser->arena = arena;
  parser->program = program;
  parser->source = source;
  parser->filename = filename;
  parser->diagnostics = diagnostics;
  parser->source_len = strlen(source);

  str_set_init(&parser->guard_names, arena);
  str_set_init(&parser->action_names, arena);
}

void parser_enter_node(Parser* parser, Node* node) {
  if(parser->node != NULL) {
    Node* parent_node = parser->node;
    parser->parent_node = parent_node;

    node_append_child(parent_node, node);
  } else {
    Program* program = parser->program;
    if(program->body == NULL) {
      program->body = node;
    } else {
      node_append_sibling(program->body, node);
    }
  }

  parser->node = node;
}

void parser_leave_node(Parser* parser) {
  Node* current_parent = parser->parent_node;
  if(current_parent != NULL) {
    Node* new_parent = current_parent->parent;
    parser->node = current_parent;
    parser->parent_node = new_parent;
  } else {
    parser->node = NULL;
  }
}

void parser_add_guard_name(Parser* parser, const char* name) {
  str_set_add(&parser->guard_names, name);
}

bool parser_has_guard_name(const Parser* parser, const char* name) {
  return str_set_contains(&parser->guard_names, name);
}

void parser_add_action_name(Parser* parser, const char* name) {
  str_set_add(&parser->action_names, name);
}

bool parser_has_action_name(const Parser* parser, const char* name) {
  return str_set_contains(&parser->action_names, name);
}

static ParseStatus consume_program(Parser* parser) {
  ParseStatus status = PARSE_OK;

  TokenType token;

  while(true) {
    token = parser_next_token(parser);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_EOF: goto end;
      case TOKEN_IDENTIFIER: {
        Keyword key = keyword_lookup(parser->word);
        if(key == KEYWORD_NONE) {
          parser_error(parser, "Unknown top-level identifier.");
          return PARSE_FATAL;
        }

        switch(key) {
          case KEYWORD_USE: {
            if(parse_merge(&status, parser_consume_use(parser)) == PARSE_FATAL) {
              return PARSE_FATAL;
            }
            break;
          }
          case KEYWORD_MACHINE: {
            if(parse_merge(&status, parser_consume_machine(parser)) == PARSE_FATAL) {
              return PARSE_FATAL;
            }
            break;
          }
          default: {
            // Top-level machine
            if(parse_merge(&status, parser_consume_implicit_machine(parser, token)) == PARSE_FATAL) {
              return PARSE_FATAL;
            }
            break;
          }
        }

        break;
      }
      default: {
        break;
      }
    }
  }

  end: {
    return status;
  }
}

ParseResult parse(Arena* arena, const char* source, const char* filename, DiagnosticList* diagnostics) {
  Program* program = program_create(arena);
  Parser parser;
  parser_init(&parser, arena, program, source, filename, diagnostics);

  ParseStatus status = consume_program(&parser);

  ParseResult result = {
    .success = status == PARSE_OK,
    .program = program
  };
  return result;
}
