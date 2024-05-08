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
  /* Опция в формате, поддерживаемом getopt_long (man 3 getopt_long). */
  struct option opt;
  /* Описание опции, которое предоставляет плагин. */
  const char *opt_descr;
};

struct plugin_info {
  /* Назначение плагина */
  const char *plugin_purpose;
  /* Автор плагина, например "Иванов Иван Иванович, N32xx" */
  const char *plugin_author;
  /* Длина списка опций */
  size_t sup_opts_len;
  /* Список опций, поддерживаемых плагином */
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

void push_plugin(struct plugin_list *list, struct loaded_plugin plugin);
void clear_plugins(struct plugin_list *list);
void filter_plugins(struct plugin_list *list);
void build_option_struct(size_t count, struct option **lops, struct plugin_list *list);
void load_plugins(char *path, struct plugin_list *list, struct option **longopts);
char *get_plugin_path(int argc, char *argv[]);
char *parse_options(int argc, char *argv[], struct option *longopts, struct plugin_list *list);


#endif // PLUGIN_API_H
