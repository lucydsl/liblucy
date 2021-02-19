#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../core/identifier.h"
#include "../core/parser.h"
#include "../core/compiler_xstate.h"

//CompileResult* compile_xstate(char*);
//char* xs_get_js(CompileResult*);

#define U_INDENT "    "

static void usage(char* program_name) {
  fprintf(stderr, "%s - Compile Lucy programs.\n\n", program_name);
  fprintf(stderr, "USAGE:\n");
  fprintf(stderr, "%s%s [FLAGS] <file>\n\n", U_INDENT, program_name);
  fprintf(stderr, "FLAGS:\n");
  fprintf(stderr, "%s-h, --help            Prints help information\n", U_INDENT);
  fprintf(stderr, "%s-v, --version         Prints the version\n", U_INDENT);
}

static void version() {
  fprintf(stderr, "0.0.1\n");
}

int main(int argc, char *argv[]) {
  identifier_init();
  parser_init();

  int opt;
  while ((opt = getopt(argc, argv, "hv")) != -1) {
    switch(opt) {
      case 'h': {
        usage(argv[0]);
        exit(0);
        break;
      }
      case 'v': {
        version();
        exit(0);
        break;
      }
      default: {
        usage(argv[0]);
        exit(1);
        break;
      }
    }
  }

  char* filename = argv[optind];

  if(filename != NULL) {
    FILE *fp;
    if ((fp = fopen(filename, "r")) == NULL){
       printf("Error! opening file");

       // Program exits if the file pointer returns NULL.
       exit(1);
    }

    char* buffer;
    long length;

    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buffer = malloc(length);
    if(buffer) {
      fread(buffer, 1, length, fp);
    }
    fclose(fp);

    CompileResult* result = compile_xstate(buffer, filename);

    if(result->success) {
      printf("%s\n", result->js);
    } else {
      fprintf(stderr, "Compilation failed!\n");
    }
  } else {
    usage(argv[0]);
  }

  return 0;
}