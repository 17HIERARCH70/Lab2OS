#include <dirent.h>
#include <dlfcn.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "plugin_api.h"
#include "file_handler.h"
#include "logger.h"

int option_A = 0;
int option_N = 0;
int option_O = 0;

#define PATH_MAX 1000

void print_version(const char *program_name) {
    LOG_DEBUG("print_version: Printing version");
    printf("%s version=1.0\n", program_name);
}

void print_help(const char *program_name, const struct plugin_list *plugins) {
    LOG_DEBUG("print_help: Printing help");

    printf("Usage: %s [OPTION] path\n", program_name);
    printf("Options:\n");
    printf("  -h\t\tPrint help\n");
    printf("  -v\t\tPrint version\n");
    printf("  -N\t\tInvert the search\n");
    printf("  -A\t\tFilter files\n");
    printf("  -O\t\tFiles that passed at least one filter\n");
    printf("  -P path\tPath to plugins directory\n");

    const struct plugin_list_node *current = plugins->head;
    while (current) {
        struct plugin_info ppi;
        int (*info)(struct plugin_info *) = dlsym(current->plugin.handle, "plugin_get_info");
        if (info && info(&ppi) == 0) {
            printf("Plugin: %s\n", ppi.plugin_purpose);
            printf("Options:\n");
            for (size_t i = 0; i < ppi.sup_opts_len; i++) {
                printf("  --%-15s %s\n", ppi.sup_opts[i].opt.name, ppi.sup_opts[i].opt_descr);
            }
        }
        current = current->next;
    }
}

void add_plugin(struct plugin_list *list, struct loaded_plugin plugin) {
    struct plugin_list_node *new_node = (struct plugin_list_node *)malloc(sizeof(struct plugin_list_node));
    new_node->plugin = plugin;
    new_node->next = list->head;
    list->head = new_node;
}

void clear_plugin_list(struct plugin_list *list) {
    while (list->head) {
        struct plugin_list_node *next = list->head->next;
        dlclose(list->head->plugin.handle);
        free(list->head->plugin.opts);
        free(list->head);
        list->head = next;
    }
}

void filter_active_plugins(struct plugin_list *list) {
    struct plugin_list_node **current = &list->head;
    while (*current) {
        if (!(*current)->plugin.flag) {
            struct plugin_list_node *to_remove = *current;
            *current = (*current)->next;
            dlclose(to_remove->plugin.handle);
            free(to_remove->plugin.opts);
            free(to_remove);
        } else {
            size_t active_opts_count = 0;
            for (size_t i = 0; i < (*current)->plugin.opts_len; i++) {
                if ((*current)->plugin.opts[i].flag) {
                    active_opts_count++;
                }
            }
            struct option *active_opts = (struct option *)malloc(active_opts_count * sizeof(struct option));
            size_t j = 0;
            for (size_t i = 0; i < (*current)->plugin.opts_len; i++) {
                if ((*current)->plugin.opts[i].flag) {
                    active_opts[j++] = (*current)->plugin.opts[i];
                }
            }
            free((*current)->plugin.opts);
            (*current)->plugin.opts = active_opts;
            (*current)->plugin.opts_len = active_opts_count;
            current = &(*current)->next;
        }
    }
}

void create_option_array(size_t count, struct option **options, struct plugin_list *list) {
    *options = (struct option *)malloc((count + 1) * sizeof(struct option));
    struct option *opt_array = *options;
    size_t index = 0;
    for (struct plugin_list_node *node = list->head; node; node = node->next) {
        for (size_t i = 0; i < node->plugin.opts_len; i++) {
            opt_array[index++] = node->plugin.opts[i];
        }
    }
    opt_array[count].name = NULL;
    opt_array[count].has_arg = 0;
    opt_array[count].flag = NULL;
    opt_array[count].val = 0;
}

void load_plugins_from_directory(const char *path, struct plugin_list *list, struct option **options) {
    LOG_DEBUG("load_plugins_from_directory: Loading plugins from %s", path);
    DIR *dir = opendir(path);
    if (!dir) {
        LOG_ERROR("load_plugins_from_directory: opendir failed for path %s", path);
        return;
    }
    struct dirent *entry;
    size_t option_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".so")) {
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            LOG_DEBUG("load_plugins_from_directory: Found plugin %s", full_path);
            void *handle = dlopen(full_path, RTLD_NOW);
            if (!handle) {
                LOG_WARN("load_plugins_from_directory: dlopen failed for %s: %s", full_path, dlerror());
                continue;
            }
            struct plugin_info ppi;
            int (*info)(struct plugin_info *) = dlsym(handle, "plugin_get_info");
            if (!info || info(&ppi) != 0) {
                LOG_WARN("load_plugins_from_directory: Failed to get plugin info for %s", full_path);
                dlclose(handle);
                continue;
            }
            struct option *opts = (struct option *)malloc(ppi.sup_opts_len * sizeof(struct option));
            for (size_t i = 0; i < ppi.sup_opts_len; i++) {
                opts[i] = ppi.sup_opts[i].opt;
                LOG_DEBUG("load_plugins_from_directory: Option %s added for plugin %s", opts[i].name, full_path);
            }
            struct loaded_plugin plugin = {
                .func = dlsym(handle, "plugin_process_file"),
                .opts_len = ppi.sup_opts_len,
                .opts = opts,
                .flag = 0,
                .handle = handle,
            };
            add_plugin(list, plugin);
            option_count += ppi.sup_opts_len;
        }
    }
    closedir(dir);
    LOG_INFO("load_plugins_from_directory: All plugins loaded");
    create_option_array(option_count, options, list);
}

char *get_plugin_directory_path(int argc, char *argv[]) {
    LOG_DEBUG("get_plugin_directory_path: Getting plugin path");
    char *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-P") == 0 && i + 1 < argc) {
            path = realpath(argv[i + 1], NULL);
            if (!path) {
                LOG_FATAL("get_plugin_directory_path: Invalid plugin path");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }
    if (!path) {
        path = realpath("./target_lib", NULL);
        if (!path) {
            path = realpath(".", NULL);
        }
    }
    return path;
}

char *parse_command_line_arguments(int argc, char *argv[], struct option *options, struct plugin_list *list) {
    LOG_DEBUG("parse_command_line_arguments: Parsing command line options");
    int opt, optindex;
    opterr = 0;
    optind = 1;
    while ((opt = getopt_long(argc, argv, "hvNOAP:", options, &optindex)) != -1) {
        switch (opt) {
            case 'h':
                print_help(argv[0], list);
                exit(EXIT_SUCCESS);
            case 'v':
                print_version(argv[0]);
                exit(EXIT_SUCCESS);
            case 'N':
                if (option_N) {
                    LOG_FATAL("parse_command_line_arguments: Duplicate option -N detected");
                    exit(EXIT_FAILURE);
                }
                option_N = 1;
                break;
            case 'O':
                if (option_O) {
                    LOG_FATAL("parse_command_line_arguments: Duplicate option -O detected");
                    exit(EXIT_FAILURE);
                }
                if (option_A) {
                    LOG_FATAL("parse_command_line_arguments: Options -O and -A cannot be used together");
                    exit(EXIT_FAILURE);
                }
                option_O = 1;
                break;
            case 'A':
                if (option_A) {
                    LOG_FATAL("parse_command_line_arguments: Duplicate option -A detected");
                    exit(EXIT_FAILURE);
                }
                if (option_O) {
                    LOG_FATAL("parse_command_line_arguments: Options -O and -A cannot be used together");
                    exit(EXIT_FAILURE);
                }
                option_A = 1;
                break;
            case 'P':
                break;
            case 0: {
                struct plugin_list_node *current = list->head;
                while (current) {
                    for (size_t i = 0; i < current->plugin.opts_len; i++) {
                        if (strcmp(options[optindex].name, current->plugin.opts[i].name) == 0) {
                            if (current->plugin.opts[i].flag) {
                                LOG_FATAL("parse_command_line_arguments: Duplicate option detected");
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
                LOG_ERROR("parse_command_line_arguments: Unknown option detected: %c", opt);
                exit(EXIT_FAILURE);
        }
    }
    if (!option_A && !option_O) {
        option_A = 1;
    }
    if (argc - optind > 1) {
        LOG_FATAL("parse_command_line_arguments: Too many arguments detected");
        exit(EXIT_FAILURE);
    } else if (argc - optind == 1) {
        return realpath(argv[optind], NULL);
    } else {
        return realpath(".", NULL);
    }
}