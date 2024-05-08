#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct plugin_option {
  struct option opt;
  const char *opt_descr;
};
struct plugin_info {
  const char *plugin_purpose;
  const char *plugin_author;
  size_t sup_opts_len;
  struct plugin_option *sup_opts;
};
static char *g_lib_name = "libagkN3246.so";
static struct plugin_option g_pi[] = {
    {{"seq-num", required_argument, NULL, 0}, "Количество последовательностей"},
    {{"seq-num-comp", required_argument, NULL, 0},
     "Оператор сравнения для количества последовательностей"},
    {{NULL, 0, NULL, 0}, NULL} // Terminate the array
};

int plugin_get_info(struct plugin_info *ppi) {
  ppi->plugin_purpose = "Поиск последователностей одинаковый байтов в файле";
  ppi->plugin_author = "Кузнецов Александр, N3246";
  ppi->sup_opts_len = 2;
  ppi->sup_opts = g_pi;
  return 0;
}
int isNumber(char *str) {
  char *endptr;
  errno = 0;
  strtol(str, &endptr, 10);
  if (errno != 0 || endptr == str) {
    return 0;
  }
  if (*endptr != '\0') {
    return 0;
  }
  return 1;
}

int plugin_process_file(const char *fname, struct option in_opts[],
                        size_t in_opts_len) {
  char *DEBUG = getenv("LAB1DEBUG");
  if (DEBUG) {
    fprintf(stderr, "DEBUG: %s: Checking file '%s'\n", g_lib_name, fname);
    for (size_t i = 0; i < in_opts_len; i++) {
      fprintf(stderr, "DEBUG: %s: Got option '%s' with arg '%s'\n", g_lib_name,
              in_opts[i].name, (char *)in_opts[i].flag);
    }
  }
  if (in_opts_len == 1 &&
      strcmp((char *)in_opts[0].flag, "seq-num-comp") == 0) {
    fprintf(stdout, "Опция seq-num-comp не работает без seq-num\n");
    fprintf(stderr, "DEBUG: %s: Option 'seq-num-comp' without 'seq-num'\n",
            g_lib_name);
    return -1;
  }
  int fd = open(fname, O_RDONLY);
  if (fd == -1) {
    if (DEBUG) {
      fprintf(stderr, "DEBUG: %s: Failed to open file '%s'\n", g_lib_name,
              fname);
    }
    return 1;
  }
  struct stat file_stat;
  stat(fname, &file_stat);
  char *data = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data == MAP_FAILED) {
    if (DEBUG) {
      if (file_stat.st_size != 0)
        fprintf(stderr, "DEBUG: %s: mmap failed\n", g_lib_name);
      else
        fprintf(stderr, "DEBUG: %s: empty file\n", g_lib_name);
    }
    return 1;
  }
  char prev = 0;
  int count = 0;
  for (off_t i = 0; i < file_stat.st_size; i++) {
    if (data[i] == prev) {
      count++;
      while (data[i]==prev && i < file_stat.st_size-1) i++;
    }
    prev = data[i];
  }
  close(fd);
  munmap(data, file_stat.st_size);
  if (!isNumber((char *)in_opts[0].flag)) {
    fprintf(stdout, "Неверный аргумент опции seq-num\n");
    if (DEBUG) {
    fprintf(stderr, "DEBUG: %s: Invalid argument for 'seq-num'\n", g_lib_name);
    }
    return -1;
  }
  int need_count = atoi((char *)in_opts[0].flag);
  if (DEBUG) {
    fprintf(stderr, "DEBUG: %s: Calculated sequence number = %d\n", g_lib_name,
            count);
  }
  if (in_opts_len == 1) {
    return (count == need_count) ? 0 : 1;
  } else {
    if (strcmp((char *)in_opts[1].flag, "eq") == 0) {
      return (count == need_count) ? 0 : 1;
    } else if (strcmp((char *)in_opts[1].flag, "ne") == 0) {
      return (count != need_count) ? 0 : 1;
    } else if (strcmp((char *)in_opts[1].flag, "gt") == 0) {
      return (count > need_count) ? 0 : 1;
    } else if (strcmp((char *)in_opts[1].flag, "lt") == 0) {
      return (count < need_count) ? 0 : 1;
    } else if (strcmp((char *)in_opts[1].flag, "ge") == 0) {
      return (count >= need_count) ? 0 : 1;
    } else if (strcmp((char *)in_opts[1].flag, "le") == 0) {
      return (count <= need_count) ? 0 : 1;
    } else {
      fprintf(stdout, "Неверный аргумент опции seq-num-comp\n");
      fprintf(stderr, "DEBUG: %s: Invalid argument for 'seq-num-comp'\n",
              g_lib_name);
      return -1;
    }
  }
}
