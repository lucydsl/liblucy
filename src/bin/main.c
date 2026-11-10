// lucyc, the command-line Lucy compiler.

// lucyc uses POSIX (stat, mkdir, opendir, PATH_MAX), which strict C17
// leaves undeclared without this.
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../core/lucy.h"
#include "../core/support/mem.h"
#include "../core/support/str_builder.h"

#define RESET     "\033[0m"
#define BOLDWHITE "\033[1m\033[37m"

#ifdef VERSION
# define PROGRAM_VERSION VERSION
#else
# define PROGRAM_VERSION "0.0.0"
#endif

#define U_INDENT "    "

// What --print prints.
typedef enum PrintTarget {
  PRINT_TARGET_JS,
  PRINT_TARGET_DTS,
} PrintTarget;

// The command line's options. The strings point into argv.
typedef struct Options {
  bool include_dts;
  const char* out_file;
  const char* out_dir;
  PrintTarget print_target;
} Options;

static void usage(const char* program_name) {
  fprintf(stderr, "%s - Compile Lucy programs.\n\n", program_name);
  fprintf(stderr, BOLDWHITE "Usage:\n" RESET);
  fprintf(stderr, "%s%s [options] [file ...]\n\n", U_INDENT, program_name);

  // Options
  fprintf(stderr, BOLDWHITE "Options:\n" RESET);
  fprintf(stderr, "%s--out-file <file>     Specify a file to output to.\n", U_INDENT);
  fprintf(stderr, "%s--out-dir <dir>       Specify a directory to output to.\n", U_INDENT);
  fprintf(stderr, "%s--experimental-dts    Include TypeScript defintion (.d.ts) files.\n"
                  "%s                       (when using --out-file or --out-dir).\n",
    U_INDENT, U_INDENT);
  fprintf(stderr, "%s--print KIND          Specify which unit to print to the console:\n"
                  "%s                       'js', 'dts'\n",
    U_INDENT, U_INDENT);
  fprintf(stderr, "%s-h, --help            Prints help information.\n", U_INDENT);
  fprintf(stderr, "%s-v, --version         Prints the version.\n\n", U_INDENT);

  // Examples
  fprintf(stderr, BOLDWHITE "Examples:\n" RESET);
  fprintf(stderr, "%s# Compile a Lucy file and print to stdout.\n", U_INDENT);
  fprintf(stderr, "%s$ %s input.lucy\n\n", U_INDENT, program_name);
  fprintf(stderr, "%s# Compile a Lucy file and output to out.js\n", U_INDENT);
  fprintf(stderr, "%s$ %s --out-file out.js input.lucy\n\n", U_INDENT, program_name);
  fprintf(stderr, "%s# Compile all Lucy files in a directory and output\n", U_INDENT);
  fprintf(stderr, "%s# them to the dist folder.\n", U_INDENT);
  fprintf(stderr, "%s$ %s --out-dir dist src\n", U_INDENT, program_name);
}

static void version(void) {
  fputs(PROGRAM_VERSION "\n", stdout);
}

// Make the directories in path, like mkdir -p. If path names a file, its
// last part is the file and isn't made. Returns 0, or -1 with errno set.
static int mkdirp(const char* path, bool is_file) {
  char copy[PATH_MAX];

  errno = 0;

  // Copy the path so that it can be cut short at each '/'.
  if(strlen(path) > sizeof(copy) - 1) {
    errno = ENAMETOOLONG;
    return -1;
  }
  strcpy(copy, path);
  // The loop starts after the first character, so that "/" isn't made,
  // and an empty path would have it start past the end.
  if(copy[0] == '\0') {
    return 0;
  }

  for(char* p = copy + 1; *p; p++) {
    if(*p == '/') {
      *p = '\0';
      if(mkdir(copy, S_IRWXU) != 0 && errno != EEXIST) {
        return -1;
      }
      *p = '/';
    }
  }

  if(!is_file && mkdir(copy, S_IRWXU) != 0 && errno != EEXIST) {
    return -1;
  }

  return 0;
}

static int write_file(const char* filename, const char* output) {
  mkdirp(filename, true);

  FILE* fp = fopen(filename, "w");
  if(fp == NULL) {
    printf("Error opening file!\n");
    return 1;
  }

  fputs(output, fp);
  fclose(fp);
  return 0;
}

// The contents of filename, NUL-terminated, or NULL if it can't be read.
// The caller must free() it.
static char* read_file(const char* filename) {
  FILE* fp = fopen(filename, "r");
  if(fp == NULL) {
    return NULL;
  }

  char* buffer = NULL;
  long length = -1;
  if(fseek(fp, 0, SEEK_END) == 0) {
    length = ftell(fp);
  }
  if(length >= 0 && fseek(fp, 0, SEEK_SET) == 0) {
    buffer = xmalloc(size_add((size_t)length, 1));
    size_t read = fread(buffer, 1, (size_t)length, fp);
    buffer[read] = '\0';
    if(ferror(fp)) {
      free(buffer);
      buffer = NULL;
    }
  }
  fclose(fp);
  return buffer;
}

// Print a compile's errors and warnings, in the order it reported them.
static void print_diagnostics(const LucyResult* result) {
  size_t count = lucy_result_diagnostic_count(result);
  for(size_t i = 0; i < count; i++) {
    const LucyDiagnostic* diagnostic = lucy_result_diagnostic(result, i);
    FILE* stream = lucy_diagnostic_stream(diagnostic) == LUCY_STREAM_STDOUT ? stdout : stderr;
    fputs(lucy_diagnostic_text(diagnostic), stream);
  }
}

// Compile filename and write the output to out_file, or print it if
// out_file is NULL. Returns 0 on success, 1 on failure.
static int compile_file(const char* filename, const char* out_file, const Options* options) {
  char* source = read_file(filename);
  if(source == NULL) {
    fprintf(stderr, "Error opening file!\n");
    return 1;
  }

  LucyResult* result = lucy_result_create();
  lucy_result_set_options(result, options->include_dts);
  lucy_compile(result, source, filename);
  print_diagnostics(result);
  // The result has its own copies of everything it needs.
  free(source);

  int ret = 0;
  if(lucy_result_success(result)) {
    if(out_file != NULL) {
      ret = write_file(out_file, lucy_result_js(result));

      if(ret == 0 && options->include_dts) {
        // out.js -> out.d.ts. A name shorter than ".js" (which used to
        // overrun the buffer) just gets ".d.ts" added.
        size_t out_len = strlen(out_file);
        size_t base_len = out_len >= 3 ? out_len - 3 : out_len;
        StrBuilder* sb = str_builder_create();
        str_builder_copy_str(sb, out_file, 0, base_len);
        str_builder_add_str(sb, ".d.ts");
        char* dts_name = str_builder_release(sb, NULL);
        ret = write_file(dts_name, lucy_result_dts(result));
        free(dts_name);
      }
    } else {
      // Without --experimental-dts there is no .d.ts to print, so print
      // the JavaScript, as scripts/lucyc.mjs does. The output ends in a
      // newline already.
      if(options->print_target == PRINT_TARGET_DTS && lucy_result_dts(result) != NULL) {
        fputs(lucy_result_dts(result), stdout);
      } else {
        fputs(lucy_result_js(result), stdout);
      }
    }
  } else {
    fprintf(stderr, "Compilation failed!\n");
    ret = 1;
  }

  lucy_result_destroy(result);
  return ret;
}

static bool ends_with(const char* str, const char* suffix) {
  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);
  return suffix_len <= str_len &&
         strcmp(str + str_len - suffix_len, suffix) == 0;
}

// filename with the rootdir prefix, and the slashes after it, removed. The
// caller must free() it.
static char* relative_name(const char* rootdir, const char* filename, size_t filename_len) {
  size_t root_len = strlen(rootdir);
  size_t i = 0;
  while(i < filename_len) {
    char ca = i < root_len ? rootdir[i] : '\0';
    char cb = filename[i];
    if(ca != cb && cb != '/') {
      break;
    }
    i++;
  }

  StrBuilder* sb = str_builder_create();
  str_builder_copy_str(sb, filename, i, filename_len);
  return str_builder_release(sb, NULL);
}

// Compile every .lucy file in dirname and the directories under it, which
// started at rootdir, into the same places under options->out_dir. Stops
// at the first that fails, and returns 1 then, else 0.
static int compile_dir(const char* rootdir, const char* dirname, const Options* options) {
  DIR* dir = opendir(dirname);
  if(dir == NULL) {
    fprintf(stderr, "Failed to open %s\n", dirname);
    return 1;
  }

  int ret = 0;
  struct dirent* entry;
  while(ret == 0 && (entry = readdir(dir)) != NULL) {
    if(entry->d_name[0] == '.') {
      continue;
    }

    StrBuilder* sb = str_builder_create();
    str_builder_add_str(sb, dirname);
    str_builder_add_char(sb, '/');
    str_builder_add_str(sb, entry->d_name);
    size_t path_len;
    char* path = str_builder_release(sb, &path_len);

    if(ends_with(entry->d_name, ".lucy")) {
      // Ends in ".lucy" too, as the rootdir prefix ends before d_name.
      char* name = relative_name(rootdir, path, path_len);

      StrBuilder* out = str_builder_create();
      str_builder_add_str(out, options->out_dir);
      str_builder_add_char(out, '/');
      str_builder_copy_str(out, name, 0, strlen(name) - strlen(".lucy"));
      str_builder_add_str(out, ".js");
      char* out_file = str_builder_release(out, NULL);

      ret = compile_file(path, out_file, options);

      free(name);
      free(out_file);
    } else {
      struct stat path_stat;
      if(stat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        ret = compile_dir(rootdir, path, options);
      }
    }

    free(path);
  }

  closedir(dir);
  return ret;
}

// getopt_long's codes for the long options.
enum {
  OPTION_OUT_FILE,
  OPTION_OUT_DIR,
  OPTION_EXPERIMENTAL_DTS,
  OPTION_PRINT,
};

static const struct option long_options[] = {
  { "experimental-dts", no_argument, NULL, OPTION_EXPERIMENTAL_DTS },
  { "out-file", required_argument, NULL, OPTION_OUT_FILE },
  { "out-dir", required_argument, NULL, OPTION_OUT_DIR },
  { "print", required_argument, NULL, OPTION_PRINT },
  { "help", no_argument, NULL, 'h' },
  { "version", no_argument, NULL, 'v' },
  // getopt_long needs the terminating entry.
  { NULL, 0, NULL, 0 },
};

int main(int argc, char* argv[]) {
  const char* program_name = argv[0];
  Options options = {
    .include_dts = false,
    .out_file = NULL,
    .out_dir = NULL,
    .print_target = PRINT_TARGET_JS
  };

  int opt;
  while((opt = getopt_long(argc, argv, "hv", long_options, NULL)) != -1) {
    switch(opt) {
      case OPTION_EXPERIMENTAL_DTS: {
        options.include_dts = true;
        break;
      }
      case OPTION_OUT_FILE: {
        options.out_file = optarg;
        break;
      }
      case OPTION_OUT_DIR: {
        options.out_dir = optarg;
        break;
      }
      case OPTION_PRINT: {
        if(strcmp(optarg, "dts") == 0) {
          options.print_target = PRINT_TARGET_DTS;
        }
        break;
      }
      case 'h': {
        usage(program_name);
        return 0;
      }
      case 'v': {
        version();
        return 0;
      }
      default: {
        usage(program_name);
        return 1;
      }
    }
  }

  // argv[argc] is NULL, so with no file this is NULL.
  const char* filename = argv[optind];
  if(filename == NULL) {
    usage(program_name);
    return 0;
  }

  struct stat path_stat;
  if(stat(filename, &path_stat) != 0) {
    fprintf(stderr, "Error opening file!\n");
    return 1;
  }

  if(S_ISDIR(path_stat.st_mode)) {
    if(options.out_file != NULL) {
      fprintf(stderr, "Compiling a directory but passed --out-file. Did you mean to use --out-dir?\n");
      return 1;
    }
    if(options.out_dir == NULL) {
      fprintf(stderr, "Compiling a directory but not provided --out-dir.\n");
      return 1;
    }
    return compile_dir(filename, filename, &options);
  }

  if(S_ISREG(path_stat.st_mode)) {
    // --out-file doesn't have to exist yet, but it mustn't be a directory.
    if(options.out_file != NULL && stat(options.out_file, &path_stat) == 0 &&
       S_ISDIR(path_stat.st_mode)) {
      fprintf(stderr, "Argument passed to --out-file is a directory. Did you mean to use --out-dir?\n");
      return 1;
    }
    return compile_file(filename, options.out_file, &options);
  }

  // Neither a file nor a directory: nothing to do.
  return 0;
}
