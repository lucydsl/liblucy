#include <stdio.h>
#include <stdbool.h>
#include "node.h"
#include "state.h"
#include "str_builder.h"

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

void error_file_info(State* state) {
  fprintf(stderr, BOLDWHITE "%s" RESET ":%zu:%zu\n", state->filename, state->line, state->index);
}

void error_message(const char* msg) {
  fprintf(stderr, "\n " BOLDRED "𝒙" RESET RED " %s\n\n" RESET, msg);
}

static void print_code_line(str_builder_t *sb, size_t line) {
  fprintf(stderr, BOLDWHITE "    %zu" RESET " │ %s", line, str_builder_dump(sb, NULL));
}

void error_annotate(State* state, Node* node) {
  char* source = state->source;
  size_t source_len = state->source_len;

  unsigned short problem_line = node->line;
  unsigned short start_line = problem_line > 2 ? (problem_line - 2) : 0;
  unsigned short end_line = problem_line + 2;

  str_builder_t *sb = str_builder_create();

  bool in_block = false;
  size_t i = 0;
  size_t line = 0;
  size_t col = 0;
  char c;
  while(i < source_len) {
    c = source[i];

    if(line == start_line) {
      in_block = true;
    }

    if(in_block) {
      str_builder_add_char(sb, c);

      if(line > end_line) {
        in_block = false;
      }
    }

    if(c == '\n') {
      if(in_block) {
        print_code_line(sb, line + 1);
        str_builder_clear(sb);
      }

      if(line == problem_line) {
        str_builder_t *psb = str_builder_create();
        unsigned short pi = 0;
        unsigned short spaces = col + 6;
        while(pi < spaces) {
          str_builder_add_char(psb, ' ');
          pi++;
        }

        fprintf(stderr, "%s" BOLDRED "˄" RESET "\n", str_builder_dump(psb, NULL));
        str_builder_destroy(psb);
      }

      line++;
      col = 0;
    } else {
      col++;
    }

    i++;
  }

  print_code_line(sb, line + 1);
  fprintf(stderr, "\n");

  str_builder_destroy(sb);
}

void error_msg_with_code_block(State* state, Node* node, const char* msg) {
  error_file_info(state);
  error_message(msg);
  error_annotate(state, node);
  printf("\n");
}

void error_unexpected_identifier(State* state, Node* node) {
  error_msg_with_code_block(state, node, "Unexpected identifier");
}