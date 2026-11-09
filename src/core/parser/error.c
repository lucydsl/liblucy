#include "internal.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#include "../support/str_builder.h"

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#define CODE_BLOCK_INDENT 4

static int num_places(long n) {
  unsigned long u = n < 0 ? -(unsigned long)n : (unsigned long)n;
  int places = 1;
  while(u >= 10) {
    u /= 10;
    places++;
  }
  return places;
}

static void error_file_info(Parser* parser, Position* pos) {
  long line = pos->line + 1;
  long col = pos->column;
  fprintf(stderr, BOLDWHITE "%s" RESET ":%li:%li\n", parser->filename, line, col);
}

void error_message(const char* msg) {
  fprintf(stderr, "\n " BOLDRED "𝒙" RESET RED " %s\n\n" RESET, msg);
}

static void print_spaces(long count) {
  for(long i = 0; i < count; i++) {
    fputc(' ', stderr);
  }
}

static void print_code_line(StrBuilder *sb, long line, int max_spaces) {
  int line_spaces = num_places(line);
  int num_spaces = max_spaces - line_spaces + 1;

  fprintf(stderr, BOLDWHITE "    %ld" RESET, line);
  print_spaces(num_spaces);
  fprintf(stderr, "│ %s", str_builder_peek(sb));
}

static void error_annotate(Parser* parser, Position* pos) {
  char* source = parser->source;
  size_t source_len = parser->source_len;

  long problem_line = pos->line;
  long problem_col =  pos->column;

  long start_line = problem_line > 2 ? (problem_line - 2) : 0;
  long end_line = problem_line + 2;
  int max_num_places = num_places(end_line);

  StrBuilder *sb = str_builder_create();

  bool in_block = false;
  size_t i = 0;
  long line = 0;
  char c;
  while(i < source_len) {
    c = source[i];

    if(line == start_line) {
      in_block = true;
    }

    if(in_block) {
      if(line > end_line) {
        in_block = false;
        break;
      }

      str_builder_add_char(sb, c);
    } 

    if(c == '\n') {
      if(in_block) {
        print_code_line(sb, line + 1, max_num_places);
        str_builder_clear(sb);
      }

      if(line == problem_line) {
        print_spaces(CODE_BLOCK_INDENT + max_num_places + 1 + problem_col + 1);
        fprintf(stderr, BOLDRED "˄" RESET "\n");
      }

      line++;
    }

    i++;
  }

  // Want at least 4 lines, so print this if not.
  if(line <= 3) {
    print_code_line(sb, line + 1, max_num_places);
  }

  fprintf(stderr, "\n");

  str_builder_destroy(sb);
}

void parser_error(Parser* parser, Node* node, const char* msg) {
  Position pos = {
    .line = (long)parser->line + 1,
    .column = (long)parser->column
  };

  error_file_info(parser, &pos);
  error_message(msg);
  error_annotate(parser, &pos);
  fprintf(stderr, "\n");
}

void parser_error_at(Parser* parser, Position* pos, const char* msg) {
  error_file_info(parser, pos);
  error_message(msg);
  error_annotate(parser, pos);
  fprintf(stderr, "\n");
}

void parser_error_back(Parser* parser, size_t dec, const char* msg) {
  Position pos;
  parser_find_position(parser, &pos, dec);
  parser_error_at(parser, &pos, msg);
}

void parser_error_unexpected_identifier(Parser* parser, Node* node) {
  parser_error(parser, node, "Unexpected identifier");
}