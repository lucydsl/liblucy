#include "parser.h"

#include "internal.h"

static int consume_program(Parser* parser) {
  int err = 0;
  Program* program = parser->program;

  TokenType token;

  while(true) {
    token = parser_next_token(parser);

    switch(token) {
      case TOKEN_EOL: continue;
      case TOKEN_EOF: goto end;
      case TOKEN_IDENTIFIER: {
        Keyword key = keyword_lookup(parser->word);
        if(key == KEYWORD_NONE) {
          parser_error(parser, parser->node, "Unknown top-level identifier.");
          return 2;
        }

        switch(key) {
          case KEYWORD_USE: {
            _check(parser_consume_use(parser));
            break;
          }
          case KEYWORD_MACHINE: {
            _check(parser_consume_machine(parser));
            break;
          }
          default: {
            // Top-level machine
            _check(parser_consume_implicit_machine(parser, token));
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
    return err;
  }
}

ParseResult parse(Arena* arena, char* source, char* filename) {
  Program* program = program_create(arena);
  Parser parser;
  parser_init(&parser, arena, program, source, filename);

  int err = consume_program(&parser);

  ParseResult result = {
    .success = err == 0,
    .program = program
  };
  return result;
}
