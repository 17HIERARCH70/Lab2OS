#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <getopt.h>
#include <stddef.h>
#include <stdio.h>

// Define option flags
extern int option_A;
extern int option_N;
extern int option_O;

struct plugin_option {
  /* Option in the format supported by getopt_long (man 3 getopt_long). */
  struct option opt;
  /* Option description provided by the plugin. */
  const char *opt_descr;
};

struct plugin_info {
  /* Purpose of the plugin */
  const char *plugin_purpose;
  /* Author of the plugin, e.g., "Ivan Ivanov, N32xx" */
  const char *plugin_author;
  /* Length of the option list */
  size_t sup_opts_len;
  /* List of options supported by the plugin */
  struct plugin_option *sup_opts;
};

struct loaded_plugin {
  int (*func)(const char *, struct option*, size_t);
  size_t opts_len;
  struct option *opts;
  char flag;
  void *handle;
};

struct plugin_list {
  struct plugin_list_node *head;
};

struct plugin_list_node {
  struct loaded_plugin plugin;
  struct plugin_list_node *next;
};

void add_plugin(struct plugin_list *list, struct loaded_plugin plugin);
void clear_plugin_list(struct plugin_list *list);
void filter_active_plugins(struct plugin_list *list);
void create_option_array(size_t count, struct option **options, struct plugin_list *list);
void load_plugins_from_directory(const char *path, struct plugin_list *list, struct option **options);
char *get_plugin_directory_path(int argc, char *argv[]);
char *parse_command_line_arguments(int argc, char *argv[], struct option *options, struct plugin_list *list);

#endif // PLUGIN_API_H
