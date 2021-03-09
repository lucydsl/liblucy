#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include "../core/identifier.h"
#include "../core/parser.h"
#include "../core/compiler_xstate.h"

#ifdef VERSION
  #define PROGRAM_VERSION VERSION
#else
  #define PROGRAM_VERSION "0.0.0"
#endif

#define U_INDENT "    "

static void usage(char* program_name) {
  fprintf(stderr, "%s - Compile Lucy programs.\n\n", program_name);
  fprintf(stderr, "USAGE:\n");
  fprintf(stderr, "%s%s [FLAGS] <file>\n\n", U_INDENT, program_name);
  fprintf(stderr, "FLAGS:\n");
  fprintf(stderr, "%s--remote-imports      Specify remote import URLs.\n", U_INDENT);
  fprintf(stderr, "%s-h, --help            Prints help information\n", U_INDENT);
  fprintf(stderr, "%s-v, --version         Prints the version\n", U_INDENT);
}

static void version() {
  fprintf(stderr, PROGRAM_VERSION);
  fprintf(stderr, "\n");
}

int compile_file(char* filename, int use_remote_imports) {
  FILE *fp;
  if ((fp = fopen(filename, "r")) == NULL){
      printf("Error! opening file");

      // Program exits if the file pointer returns NULL.
      return 1;
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

  CompileResult* result = xs_create();
  xs_init(result, use_remote_imports);
  compile_xstate(result, buffer, filename);

  if(result->success) {
    printf("%s\n", result->js);
    destroy_xstate_result(result);
    return 0;
  } else {
    fprintf(stderr, "Compilation failed!\n");
    return 1;
  }
}

static struct option long_options[] = {
  {"remote-imports", no_argument, 0, 0},
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'v'},
};

int main(int argc, char *argv[]) {
  identifier_init();
  parser_init();

  int use_remote_imports = 0;

  int option_index = 0;
  int opt;
  while ((opt = getopt_long(argc, argv, "hv", long_options, &option_index)) != -1) {
    switch(opt) {
      case 0: {
        use_remote_imports = 1;
        break;
      }
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

  char* program_name = argv[0];
  char* filename = argv[optind];

  if(filename != NULL) {
    struct stat path_stat;
    stat(filename, &path_stat);

    if(S_ISDIR(path_stat.st_mode)) {
      printf("Compiling directories is not currently supported.\n\n");
      usage(program_name);
      return 1;
    } else if(S_ISREG(path_stat.st_mode)) {
      int ret = compile_file(filename, use_remote_imports);
      return ret;
    }
  } else {
    usage(program_name);
  }

  return 0;
}