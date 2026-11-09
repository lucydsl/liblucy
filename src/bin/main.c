// lucyc uses POSIX (stat, mkdir, opendir, PATH_MAX), which strict C17
// leaves undeclared without this.
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h>     /* PATH_MAX */
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include "../core/lucy.h"
#include "../core/support/mem.h"
#include "../core/support/str_builder.h"

#define RESET   "\033[0m"
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#ifdef VERSION
  #define PROGRAM_VERSION VERSION
#else
  #define PROGRAM_VERSION "0.0.0"
#endif

#define U_INDENT "    "

static void usage(const char* program_name) {
  fprintf(stderr, "%s - Compile Lucy programs.\n\n", program_name);
  fprintf(stderr, BOLDWHITE "Usage:\n" RESET);
  fprintf(stderr, "%s%s [options] [file ...]\n\n", U_INDENT, program_name);

  // Options
  fprintf(stderr, BOLDWHITE "Options:\n" RESET);
  fprintf(stderr, "%s--out-file <file>     Specify a file to output to.\n", U_INDENT);
  fprintf(stderr, "%s--out-dir <dir>       Specify a directory to output to.\n", U_INDENT);
  fprintf(stderr, "%s--remote-imports      Specify remote import URLs.\n", U_INDENT);
  fprintf(stderr, "%s--experimental-dts    Include TypeScript defintion (.d.ts) files.\n"
                  "%s                       (when using --out-file or --out-dir).\n", U_INDENT, U_INDENT);
  fprintf(stderr, "%s--print KIND          Specify which unit to print to the console:\n"
                  "%s                       'js', 'dts'\n", U_INDENT, U_INDENT);
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

// What --print prints.
typedef enum PrintTarget {
  PRINT_TARGET_JS,
  PRINT_TARGET_DTS,
} PrintTarget;

static int mkdirp(const char *path, bool is_file) {
    const size_t len = strlen(path);
    char _path[PATH_MAX];
    char *p; 

    errno = 0;

    /* Copy string so its mutable */
    if (len > sizeof(_path)-1) {
        errno = ENAMETOOLONG;
        return -1; 
    }   
    strcpy(_path, path);

    /* Iterate the string */
    for (p = _path + 1; *p; p++) {
        if (*p == '/') {
            /* Temporarily truncate */
            *p = '\0';

            if (mkdir(_path, S_IRWXU) != 0) {
                if (errno != EEXIST)
                    return -1; 
            }

            *p = '/';
        }
    }   

    if(!is_file) {
      if (mkdir(_path, S_IRWXU) != 0) {
          if (errno != EEXIST)
              return -1; 
      }
    }

    return 0;
}

static int write_file(const char* outfile, const char* output) {
  FILE *fp;

  mkdirp(outfile, true);
  if((fp = fopen(outfile, "w")) == NULL) {
    printf("Error opening file!\n");

    return 1;
  }

  fputs(output, fp);
  fclose(fp);
  return 0;
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

// The contents of filename, NUL-terminated, or NULL if it can't be read.
// The caller must free() it.
static char* read_file(const char* filename) {
  FILE *fp = fopen(filename, "r");
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

static int compile_file(const char* filename, bool use_remote_imports, bool include_dts,
  const char* out_file, PrintTarget print_target) {
  char* buffer = read_file(filename);
  if(buffer == NULL) {
    fprintf(stderr, "Error opening file!\n");
    return 1;
  }

  LucyResult* result = lucy_result_create();
  lucy_result_set_options(result, use_remote_imports, include_dts);
  lucy_compile_xstate(result, buffer, filename);
  print_diagnostics(result);
  // The result has its own copies of everything it needs.
  free(buffer);

  int ret = 0;
  if(lucy_result_success(result)) {
    if(out_file != NULL) {
      ret = write_file(out_file, lucy_result_js(result));

      if(ret == 0 && include_dts) {
        // out.js -> out.d.ts. This took outlen - 3 unsigned, so a name
        // shorter than ".js" wrapped around and overran the buffer.
        size_t outlen = strlen(out_file);
        size_t base_len = outlen >= 3 ? outlen - 3 : outlen;
        StrBuilder* sb = str_builder_create();
        str_builder_copy_str(sb, out_file, 0, base_len);
        str_builder_add_str(sb, ".d.ts");
        char* dtsname = str_builder_release(sb, NULL);
        ret = write_file(dtsname, lucy_result_dts(result));
        free(dtsname);
      }
    } else {
      // Without --experimental-dts there is no d.ts to print, so print the
      // JavaScript, as scripts/lucyc.mjs does. This used to pass NULL to
      // printf.
      if(print_target == PRINT_TARGET_DTS && lucy_result_dts(result) != NULL) {
        printf("%s\n", lucy_result_dts(result));
      } else {
        printf("%s\n", lucy_result_js(result));
      }
    }
  } else {
    fprintf(stderr, "Compilation failed!\n");
    ret = 1;
  }

  lucy_result_destroy(result);
  return ret;
}

static bool ends_with(const char *str, const char *suffix) {
    if (!str || !suffix)
        return false;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return false;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

// filename with the rootdir prefix, and the slashes after it, removed. The
// caller must free() it. (This was called basename, which is also a POSIX
// function, and it read rootdir past its end.)
static char* relative_name(const char* rootdir, const char* filename, size_t fnlen) {
  size_t rootlen = strlen(rootdir);
  size_t i = 0;
  while(i < fnlen) {
    char ca = i < rootlen ? rootdir[i] : '\0';
    char cb = filename[i];
    if(ca != cb && cb != '/') {
      break;
    }
    i++;
  }

  StrBuilder *sb = str_builder_create();
  str_builder_copy_str(sb, filename, i, fnlen);
  return str_builder_release(sb, NULL);
}

static int compile_dir(const char* rootdir, const char* dirname,
  bool use_remote_imports, bool include_dts, const char* out_dir,
  PrintTarget print_target) {
  DIR* fd;
  struct dirent* in_file;
  int ret = 0;

  if (NULL == (fd = opendir(dirname))) {
    fprintf(stderr, "Failed to open %s\n", dirname);
    return 1;
  }

  while(ret == 0 && (in_file = readdir(fd))) {
    if(in_file->d_name[0] == '.') {
      continue;
    }

    StrBuilder *sb = str_builder_create();
    str_builder_add_str(sb, dirname);
    str_builder_add_char(sb, '/');
    str_builder_add_str(sb, in_file->d_name);

    size_t fplen;
    char* fp = str_builder_release(sb, &fplen);

    if(ends_with(in_file->d_name, ".lucy")) {
      char* bname = relative_name(rootdir, fp, fplen);
      size_t bnamelen = strlen(bname);

      StrBuilder *sbb = str_builder_create();
      str_builder_add_str(sbb, out_dir);
      str_builder_add_char(sbb, '/');
      str_builder_copy_str(sbb, bname, 0, bnamelen - 5); // without .lucy
      str_builder_add_str(sbb, ".js");
      char* outfp = str_builder_release(sbb, NULL);

      ret = compile_file(fp, use_remote_imports, include_dts, outfp, print_target);

      free(bname);
      free(outfp);
    } else {
      // If directory, recurse. (stat's result wasn't checked, so a failed
      // stat left path_stat uninitialised.)
      struct stat path_stat;
      if(stat(fp, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        ret = compile_dir(rootdir, fp, use_remote_imports, include_dts,
          out_dir, print_target);
      }
    }

    free(fp);
  }

  closedir(fd);
  return ret;
}

// getopt_long's codes for the long options.
enum {
  OPTION_REMOTE_IMPORTS,
  OPTION_OUT_FILE,
  OPTION_OUT_DIR,
  OPTION_EXPERIMENTAL_DTS,
  OPTION_PRINT,
};

static struct option long_options[] = {
  {"remote-imports", no_argument, 0, OPTION_REMOTE_IMPORTS},
  {"experimental-dts", no_argument, 0, OPTION_EXPERIMENTAL_DTS},
  {"out-file", required_argument, 0, OPTION_OUT_FILE},
  {"out-dir", required_argument, 0, OPTION_OUT_DIR},
  {"print", required_argument, 0, OPTION_PRINT},
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'v'},
  // getopt_long needs the terminating entry; without it it read past the
  // end of the array.
  {0, 0, 0, 0},
};

int main(int argc, char* argv[]) {
  bool use_remote_imports = false;
  bool include_dts = false;

  // These point into argv, which outlives everything here.
  char* out_file = NULL;
  char* out_dir = NULL;
  PrintTarget print_target = PRINT_TARGET_JS;

  int option_index = 0;
  int opt;
  while ((opt = getopt_long(argc, argv, "hv", long_options, &option_index)) != -1) {
    switch(opt) {
      case OPTION_REMOTE_IMPORTS: {
        use_remote_imports = true;
        break;
      }
      case OPTION_EXPERIMENTAL_DTS: {
        include_dts = true;
        break;
      }
      case OPTION_OUT_FILE: {
        out_file = optarg;
        break;
      }
      case OPTION_OUT_DIR: {
        out_dir = optarg;
        break;
      }
      case OPTION_PRINT: {
        if(strcmp(optarg, "dts") == 0) {
          print_target = PRINT_TARGET_DTS;
        }
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

  const char* program_name = argv[0];
  const char* filename = argv[optind];

  if(filename != NULL) {
    struct stat path_stat;
    if(stat(filename, &path_stat) != 0) {
      fprintf(stderr, "Error opening file!\n");
      return 1;
    }

    if(S_ISDIR(path_stat.st_mode)) {
      if(out_file != NULL) {
        fprintf(stderr, "Compiling a directory but passed --out-file. Did you mean to use --out-dir?\n");
        return 1;
      }

      if(out_dir == NULL) {
        fprintf(stderr, "Compiling a directory but not provided --out-dir.\n");
        return 1;
      }

      int ret = compile_dir(filename, filename, use_remote_imports,
        include_dts, out_dir, print_target);
      return ret;
    } else if(S_ISREG(path_stat.st_mode)) {
      // Check if the out_file is a directory.
      if(out_file != NULL) {
        // It doesn't have to exist yet.
        if(stat(out_file, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
          fprintf(stderr, "Argument passed to --out-file is a directory. Did you mean to use --out-dir?\n");
          return 1;
        }
      }

      int ret = compile_file(filename, use_remote_imports, include_dts,
        out_file, print_target);
      return ret;
    }
  } else {
    usage(program_name);
  }

  return 0;
}