#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include "../core/identifier.h"
#include "../core/parser.h"
#include "../core/compiler_xstate.h"
#include "../core/error.h"

#define RESET   "\033[0m"
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#ifdef VERSION
  #define PROGRAM_VERSION VERSION
#else
  #define PROGRAM_VERSION "0.0.0"
#endif

#define U_INDENT "    "

static void usage(char* program_name) {
  fprintf(stderr, "%s - Compile Lucy programs.\n\n", program_name);
  fprintf(stderr, BOLDWHITE "Usage:\n" RESET);
  fprintf(stderr, "%s%s [options] [file ...]\n\n", U_INDENT, program_name);

  // Options
  fprintf(stderr, BOLDWHITE "Options:\n" RESET);
  fprintf(stderr, "%s--out-file <file>     Specify a file to output to.\n", U_INDENT);
  fprintf(stderr, "%s--out-dir <dir>       Specify a directory to output to.\n", U_INDENT);
  fprintf(stderr, "%s--remote-imports      Specify remote import URLs.\n", U_INDENT);
  fprintf(stderr, "%s-h, --help            Prints help information.\n", U_INDENT);
  fprintf(stderr, "%s-v, --version         Prints the version.\n\n", U_INDENT);

  // Examples
  fprintf(stderr, BOLDWHITE "Examples:\n" RESET);
  fprintf(stderr, "%s# Compile a Lucy file and print to stdout.\n", U_INDENT);
  fprintf(stderr, "%s$ %s input.lucy\n\n", U_INDENT, program_name);
  fprintf(stderr, "%s# Compile a Lucy file and output to out.js\n", U_INDENT);
  fprintf(stderr, "%s$ %s --out-file out.js input.lucy\n", U_INDENT, program_name);
}

static void version() {
  fprintf(stderr, PROGRAM_VERSION);
  fprintf(stderr, "\n");
}

int write_file(char* outfile, char* output) {
  FILE *fp;

  if((fp = fopen(outfile, "w")) == NULL) {
    printf("Error opening file!\n");

    return 1;
  }

  fputs(output, fp);
  fclose(fp);
  return 0;
}

int compile_file(char* filename, int use_remote_imports, char* out_file) {
  FILE *fp;
  if ((fp = fopen(filename, "r")) == NULL) {
      printf("Error opening file!\n");

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
    int ret = 0;
    if(out_file != NULL) {
      ret = write_file(out_file, result->js);
    } else {
      printf("%s\n", result->js);
    }

    destroy_xstate_result(result);
    return ret;
  } else {
    fprintf(stderr, "Compilation failed!\n");
    return 1;
  }
}

#define OPTION_REMOTE_IMPORTS 0
#define OPTION_OUT_FILE 1
#define OPTION_OUT_DIR 2

static struct option long_options[] = {
  {"remote-imports", no_argument, 0, OPTION_REMOTE_IMPORTS},
  {"out-file", required_argument, 0, OPTION_OUT_FILE},
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'v'},
};

int main(int argc, char *argv[]) {
  identifier_init();
  parser_init();

  int use_remote_imports = 0;
  char* out_file = NULL;

  int option_index = 0;
  int opt;
  while ((opt = getopt_long(argc, argv, "hv", long_options, &option_index)) != -1) {
    switch(opt) {
      case 0: {
        use_remote_imports = 1;
        break;
      }
      case 1: {
        out_file = strdup(optarg);
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
      // Check if the out_file is a directory.
      if(out_file != NULL) {
        stat(out_file, &path_stat);

        if(S_ISDIR(path_stat.st_mode)) {
          printf("Argument passed to --out-file is a directory. Did you mean to use --out-dir?\n");
          return 1;
        }
      }

      int ret = compile_file(filename, use_remote_imports, out_file);
      return ret;
    }
  } else {
    usage(program_name);
  }

  return 0;
}