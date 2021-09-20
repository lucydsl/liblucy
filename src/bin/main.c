#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h>     /* PATH_MAX */
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include "../core/identifier.h"
#include "../core/parser/parser.h"
#include "../core/xstate/compiler.h"
#include "../core/error.h"
#include "../core/str_builder.h"

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
  fprintf(stderr, "%s--no-dts              Do not include DTS files.\n"
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

static void version() {
  fprintf(stdout, PROGRAM_VERSION);
  fprintf(stdout, "\n");
}

#define PRINT_TARGET_JS 0
#define PRINT_TARGET_DTS 1

int mkdirp(const char *path, int is_file) {
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

int write_file(char* outfile, char* output) {
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

int compile_file(char* filename, int use_remote_imports, int include_dts,
  char* out_file, unsigned long outlen, int print_target) {
  FILE *fp;
  if ((fp = fopen(filename, "r")) == NULL) {
      fprintf(stderr, "Error opening file!\n");

      // Program exits if the file pointer returns NULL.
      return 1;
  }

  char* buffer;
  long length;

  fseek(fp, 0, SEEK_END);
  length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  buffer = malloc(length + 1);
  if(buffer) {
    fread(buffer, 1, length, fp);
    buffer[length] = 0;
  }
  fclose(fp);

  CompileResult* result = xs_create();
  xs_init(result, use_remote_imports, include_dts);
  compile_xstate(result, buffer, filename);

  if(result->success) {
    int ret = 0;
    if(out_file != NULL) {
      ret = write_file(out_file, result->js);

      if(ret == 0 && include_dts) {
        unsigned long dtslen = outlen + 2; // .d.ts vs .js
        char* dtsname = malloc(sizeof(char) * dtslen);
        size_t i = 0;
        for(; i < outlen - 3; i++) {
          dtsname[i] = out_file[i];
        }
        dtsname[i++] = '.';
        dtsname[i++] = 'd';
        dtsname[i++] = '.';
        dtsname[i++] = 't';
        dtsname[i++] = 's';
        dtsname[i] = '\0';
        ret = write_file(dtsname, result->dts);
        free(dtsname);
      }
    } else {
      if(print_target == PRINT_TARGET_JS) {
        printf("%s\n", result->js);
      } else if(print_target == PRINT_TARGET_DTS) {
        printf("%s\n", result->dts);
      }
    }

    destroy_xstate_result(result);
    return ret;
  } else {
    fprintf(stderr, "Compilation failed!\n");
    return 1;
  }
}

int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

char* basename(char* rootdir, char* filename, unsigned long fnlen) {
  str_builder_t *sb = str_builder_create();
  size_t i = 0;
  size_t len = 0;
  int in_root = 1;
  while(i < fnlen) {
    char cb = filename[i];
    if(in_root) {
      char ca = rootdir[i];
      if(ca == cb || cb == '/') {
        i++;
        continue;
      }
      in_root = false;
    }
    str_builder_add_char(sb, cb);
    i++;
    len++;
  }
  char* out = str_builder_dump(sb, &len);
  return out;
}

int compile_dir(char* rootdir, char* dirname, unsigned long dirlen,
  int use_remote_imports, int include_dts, char* out_dir, unsigned long outdirlen,
  int print_target) {
  DIR* fd;
  struct dirent* in_file;

  if (NULL == (fd = opendir(dirname))) {
    fprintf(stderr, "Failed to open %s\n", dirname);
    return 1;
  }

  while((in_file = readdir(fd))) {
    if(in_file->d_name[0] == '.') {
      continue;
    }

    str_builder_t *sb = str_builder_create();
    str_builder_add_str(sb, dirname, dirlen);
    str_builder_add_char(sb, '/');
    str_builder_add_str(sb, in_file->d_name, strlen(in_file->d_name));
    
    unsigned long fplen = dirlen + 1 + strlen(in_file->d_name);
    char* fp = str_builder_dump(sb, &fplen);    

    if(ends_with(in_file->d_name, ".lucy")) {
      char* bname = basename(rootdir, fp, fplen);
      unsigned long bnamelen = strlen(bname);

      str_builder_t *sbb = str_builder_create();
      str_builder_add_str(sbb, out_dir, outdirlen);
      str_builder_add_char(sbb, '/');
      str_builder_add_str(sbb, bname, bnamelen - 5);
      str_builder_add_str(sbb, ".js", 3);
      unsigned long outlen = outdirlen + 1 + bnamelen - 2; // .lucy - .js
      char* outfp = str_builder_dump(sbb, &outlen);

      int ret = compile_file(fp, use_remote_imports, include_dts, outfp, outlen, print_target);

      free(fp);
      free(bname);
      free(outfp);

      if(ret) {
        return ret;
      }

      continue;
    }

    // If directory, recurse.
    struct stat path_stat;
    stat(fp, &path_stat);

    if(S_ISDIR(path_stat.st_mode)) {
      int ret = compile_dir(rootdir, fp, fplen, use_remote_imports, include_dts,
        out_dir, outdirlen, print_target);

      if(ret) {
        free(fp);
        return ret;
      }
    }

    free(fp);
  }

  return 0;
}

#define OPTION_REMOTE_IMPORTS 0
#define OPTION_OUT_FILE 1
#define OPTION_OUT_DIR 2
#define OPTION_NO_DTS 3
#define OPTION_PRINT 4

static struct option long_options[] = {
  {"remote-imports", no_argument, 0, OPTION_REMOTE_IMPORTS},
  { "no-dts", no_argument, 0, OPTION_NO_DTS},
  {"out-file", required_argument, 0, OPTION_OUT_FILE},
  {"out-dir", required_argument, 0, OPTION_OUT_DIR},
  {"print", required_argument, 0, OPTION_PRINT},
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'v'},
};

int main(int argc, char *argv[]) {
  identifier_init();
  parser_init();

  int use_remote_imports = 0;
  int include_dts = 1;

  char* out_file = NULL;
  unsigned long out_file_len = 0;
  char* out_dir = NULL;
  int print_target = 0;

  int option_index = 0;
  int opt;
  while ((opt = getopt_long(argc, argv, "hv", long_options, &option_index)) != -1) {
    switch(opt) {
      case OPTION_REMOTE_IMPORTS: {
        use_remote_imports = 1;
        break;
      }
      case OPTION_NO_DTS: {
        include_dts = 0;
        break;
      }
      case OPTION_OUT_FILE: {
        out_file = strdup(optarg);
        out_file_len = strlen(out_file);
        break;
      }
      case OPTION_OUT_DIR: {
        out_dir = strdup(optarg);
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

  char* program_name = argv[0];
  char* filename = argv[optind];

  if(filename != NULL) {
    struct stat path_stat;
    stat(filename, &path_stat);

    if(S_ISDIR(path_stat.st_mode)) {
      if(out_file != NULL) {
        fprintf(stderr, "Compiling a directory but passed --out-file. Did you mean to use --out-dir?\n");
        return 1;
      }

      if(out_dir == NULL) {
        fprintf(stderr, "Compiling a directory but not provided --out-dir.\n");
        return 1;
      }

      unsigned long dir_len = strlen(filename);
      unsigned long out_dir_len = strlen(out_dir);
      int ret = compile_dir(filename, filename, dir_len, use_remote_imports,
        include_dts, out_dir, out_dir_len, print_target);
      return ret;
    } else if(S_ISREG(path_stat.st_mode)) {
      // Check if the out_file is a directory.
      if(out_file != NULL) {
        stat(out_file, &path_stat);

        if(S_ISDIR(path_stat.st_mode)) {
          fprintf(stderr, "Argument passed to --out-file is a directory. Did you mean to use --out-dir?\n");
          return 1;
        }
      }

      int ret = compile_file(filename, use_remote_imports, include_dts,
        out_file, out_file_len, print_target);
      return ret;
    }
  } else {
    usage(program_name);
  }

  return 0;
}