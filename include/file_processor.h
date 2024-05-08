#ifndef FILE_PROCESSOR_H
#define FILE_PROCESSOR_H

#include "plugin_api.h"

void process_directory_files(char *path, struct plugin_list *list);
int compare_flags(int a, int b);

#endif /* FILE_PROCESSOR_H */

