#include <dirent.h>
#include <dlfcn.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "plugin_api.h"
#include "file_processor.h"
#include "logger.h"

int option_A = 0;
int option_N = 0;
int option_O = 0;

void print_version(char *prog_name) {
  LOG_DEBUG("print_version: Printing version");
  printf("%s version=1.0", prog_name);
}

void print_help(char *prog_name, struct plugin_list *plugins) {
  LOG_DEBUG("print_help: Printing help");

  printf("Usage: %s [OPTION] path\n", prog_name);
  printf("Options:\n");
  printf("  -h\t\tprint help\n");
  printf("  -v\t\tprint version\n");
  printf("  -N\t\tinverts the search\n");
  printf("  -A\t\tfiltered files\n");
  printf("  -O\t\tfiles that passed at least one filter\n");
  printf("  -P path\t\tPath to plugins directory\n");

  struct plugin_list_node *current = plugins->head;

  while (current) {
    struct plugin_info *ppi = malloc(sizeof(struct plugin_info));
    if (!ppi) {
      LOG_ERROR("print_help: Memory allocation failed");
      exit(EXIT_FAILURE);
    }

    int (*info)(struct plugin_info *ppi) = dlsym(current->plugin.handle, "plugin_get_info");
    if (!info) {
      LOG_WARN("print_help: Failed to get plugin info for plugin");
      current = current->next;
      free(ppi);
      continue;
    }

    info(ppi);

    printf("Plugin: %s\n", ppi->plugin_purpose);
    printf("Options:\n");
    for (size_t i = 0; i < ppi->sup_opts_len; i++) {
      printf("  --%-15s", ppi->sup_opts[i].opt.name);
      if (ppi->sup_opts[i].opt.has_arg == required_argument) {
        printf(" <arg>");
      } else {
        printf("      ");
      }
      printf("\t%s\n", ppi->sup_opts[i].opt_descr);
    }

    current = current->next;
    free(ppi);
  }
}


void push_plugin(struct plugin_list *list, struct loaded_plugin plugin) {
  LOG_DEBUG("push_plugin: Pushing plugin to list");
  if (list->head) {
    struct plugin_list_node *current = list->head;
    while (current->next) {
      current = current->next;
    }
    current->next = (struct plugin_list_node *)malloc(sizeof(struct plugin_list_node));
    current->next->plugin = plugin;
    current->next->next = NULL;
  } else {
    list->head = (struct plugin_list_node *)malloc(sizeof(struct plugin_list_node));
    list->head->plugin = plugin;
    list->head->next = NULL;
  }
}

void clear_plugins(struct plugin_list *list) {
  LOG_DEBUG("clear_plugins: Clearing list of plugins");
  struct plugin_list_node *current = list->head;
  while (current) {
    struct plugin_list_node *next = current->next;
    dlclose(current->plugin.handle);
    free(current->plugin.opts);
    free(current);
    current = next;
  }
  list->head = NULL;
}

void filter_plugins(struct plugin_list *list) {
  LOG_DEBUG("filter_plugins: Filtering plugins");
  struct plugin_list_node *current = list->head;
  struct plugin_list_node *prev = NULL;
  while (current) {
    struct plugin_list_node *next = current->next;
    if (!(current->plugin.flag)) {
      if (current == list->head) {
        list->head = next;
      } else {
        prev->next = next;
      }
      free(current->plugin.opts);
      dlclose(current->plugin.handle);
      free(current);
    } else {
      size_t count = 0; // Change loop index type to size_t
      for (size_t i = 0; i < current->plugin.opts_len; i++) { // Change loop index type to size_t
        if (current->plugin.opts[i].flag) {
          count++;
        }
      }
      struct option *new_opts = (struct option *)malloc(count * sizeof(struct option));
      current->plugin.opts_len = count;
      size_t j = 0; // Change loop index type to size_t
      for (size_t i = 0; i < current->plugin.opts_len; i++) { // Change loop index type to size_t
        if (current->plugin.opts[i].flag) {
          new_opts[j++] = current->plugin.opts[i];
        }
      }
      free(current->plugin.opts);
      current->plugin.opts = new_opts;
      prev = current;
    }
    current = next;
  }
}

void build_option_struct(size_t count, struct option **lops, struct plugin_list *list) {
  LOG_DEBUG("build_option_struct: Building option structure");
  *lops = (struct option *)malloc((count + 1) * sizeof(struct option));
  struct option *longopts = *lops;
  struct plugin_list_node *current = list->head;
  size_t k = 0;  
  while (current) {
    for (size_t i = 0; i < current->plugin.opts_len; i++) {   
      longopts[k++] = current->plugin.opts[i];
    }
    current = current->next;
  }
  longopts[count].name = NULL;
  longopts[count].has_arg = 0;
  longopts[count].flag = NULL;
  longopts[count].val = 0;
}

void load_plugins(char *path, struct plugin_list *list, struct option **longopts) {
    LOG_DEBUG("load_plugins: Loading plugins from %s", path);
    size_t option_total_count = 0;
    DIR *dir = opendir(path);
    if (!dir) {
        LOG_ERROR("load_plugins: opendir failed for path %s", path);
        return;
    }
    (void)readdir(dir);
    (void)readdir(dir);
    struct dirent *file;
    while ((file = readdir(dir)) != NULL) {
        if (file->d_type == DT_REG) {
            char *name = file->d_name;
            if (strstr(name, ".so")) {
                char *full_name = (char *)malloc(strlen(path) + strlen(name) + 2);
                snprintf(full_name, strlen(path) + strlen(name) + 2, "%s/%s", path, name); // Use snprintf for string concatenation
                LOG_DEBUG("load_plugins: Found plugin %s", full_name);
                void *handle = dlopen(full_name, RTLD_NOW);
                if (!handle) {
                    LOG_WARN("load_plugins: dlopen failed for %s: %s", full_name, dlerror());
                    continue;
                }
                int (*info)(struct plugin_info *ppi) = dlsym(handle, "plugin_get_info");
                if (!info) {
                    LOG_WARN("load_plugins: dlsym(plugin_get_info) failed for %s: %s", full_name, dlerror());
                    dlclose(handle);
                    free(full_name);
                    continue;
                }
                struct plugin_info *ppi = (struct plugin_info *)malloc(sizeof(struct plugin_info));
                info(ppi);
                struct option *opt = malloc(sizeof(struct option) * ppi->sup_opts_len);
                for (size_t i = 0; i < ppi->sup_opts_len; i++) { // Change loop index type to size_t
                    opt[i] = ppi->sup_opts[i].opt;
                    LOG_DEBUG("load_plugins: Option %s added for plugin %s", ppi->sup_opts[i].opt.name, full_name);
                }
                struct loaded_plugin plug = {
                    .func = dlsym(handle, "plugin_process_file"),
                    .opts_len = ppi->sup_opts_len,
                    .opts = opt,
                    .flag = 0,
                    .handle = handle,
                };
                push_plugin(list, plug);
                option_total_count += ppi->sup_opts_len;
                free(full_name);
                free(ppi);
                LOG_DEBUG("load_plugins: Plugin loaded: %s", full_name);
            }
        }
    }
    closedir(dir);
    free(path);
    LOG_INFO("load_plugins: All plugins loaded");
    LOG_DEBUG("load_plugins: Total options count = %ld", option_total_count);
    build_option_struct(option_total_count, longopts, list);
}

char *get_plugin_path(int argc, char *argv[]) {
  LOG_DEBUG("get_plugin_path: Getting plugin path");

  int flag = 0;
  char *path = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-P") == 0) {
      flag = 1;
      if (path) {
        LOG_FATAL("get_plugin_path: Duplicate option");
        exit(EXIT_FAILURE);
      }
      if (i + 1 < argc) {
        path = realpath(argv[i + 1], NULL);
        if (!path) {
          LOG_FATAL("get_plugin_path: Incorrect plugin path");
          exit(EXIT_FAILURE);
        }
      }
    }
  }

  if (!path && flag) {
    LOG_FATAL("get_plugin_path: -P option without argument");
    exit(EXIT_FAILURE);
  }

  path = path ? path : realpath("./target_lib", NULL);
  path = path ? path : realpath(".", NULL);

  return path;
}

char *parse_options(int argc, char *argv[], struct option *longopts, struct plugin_list *list) {
  LOG_DEBUG("parse_options: Parsing options");
  int opt, optindex;
  opterr = 0;
  optind = 1;
  while ((opt = getopt_long(argc, argv, "hvNOAP:", longopts, &optindex)) != -1) {
    switch (opt) {
      case 'h':
        LOG_DEBUG("parse_options: Option -h detected");
        print_help(argv[0], list);
        exit(EXIT_SUCCESS);
      case 'v':
        LOG_DEBUG("parse_options: Option -v detected");
        print_version(argv[0]);
        exit(EXIT_SUCCESS);
      case 'N':
        LOG_DEBUG("parse_options: Option -N detected");
        if (option_N) {
          LOG_FATAL("parse_options: Duplicate option detected");
          exit(EXIT_FAILURE);
        }
        option_N = 1;
        break;
      case 'O':
        LOG_DEBUG("parse_options: Option -O detected");
        if (option_O) {
          LOG_FATAL("parse_options: Duplicate option detected");
          exit(EXIT_FAILURE);
        }
        if (option_A) {
            LOG_INFO("parse_options: Option -O detected");
          LOG_FATAL("parse_options: Options -O and -A cannot be used together");
          exit(EXIT_FAILURE);
        }
        option_O = 1;
        break;
      case 'A':
        LOG_DEBUG("parse_options: Option -A detected");
        if (option_A) {
          LOG_FATAL("parse_options: Duplicate option detected");
          exit(EXIT_FAILURE);
        }
        if (option_O) {
          LOG_FATAL("parse_options: Options -O and -A cannot be used together");
          exit(EXIT_FAILURE);
        }
        option_A = 1;
        break;
      case 'P':
        // Already processed
        break;
      case 0: {
        LOG_DEBUG("parse_options: Custom option detected: %s with arg %s", longopts[optindex].name, optarg);
        struct plugin_list_node *current = list->head;
        while (current) {
          for (size_t i = 0; i < current->plugin.opts_len; i++) {
            if (strcmp(longopts[optindex].name, current->plugin.opts[i].name) == 0) {
              if (current->plugin.opts[i].flag) {
                LOG_FATAL("parse_options: Duplicate option detected");
                exit(EXIT_FAILURE);
              }
              current->plugin.flag = 1;
              current->plugin.opts[i].flag = (int *)optarg;
              break;
            }
          }
          current = current->next;
        }
        break;
      }
      default:
        LOG_ERROR("parse_options: Unknown option detected: %c", opt);
        exit(EXIT_FAILURE);
    }
  }
  if (!option_A && !option_O) {
    option_A = 1;
  }
  if (argc - optind > 1) {
    LOG_FATAL("parse_options: Too many arguments detected");
    exit(EXIT_FAILURE);
  } else if (argc - optind == 1) {
    return realpath(argv[optind], NULL);
  } else {
    return realpath(".", NULL);
  }
}


