#include "parser.h"

#include "internal.h"

static ParseStatus consume_program(Parser* parser) {
  ParseStatus status = PARSE_OK;
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

ParseResult parse(Arena* arena, char* source, char* filename) {
  Program* program = program_create(arena);
  Parser parser;
  parser_init(&parser, arena, program, source, filename);

  ParseStatus status = consume_program(&parser);

  ParseResult result = {
    .success = status == PARSE_OK,
    .program = program
  };
  return result;
}
