#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include "plugin_api.h"

void handle_directory_files(char *directory_path, struct plugin_list *plugins);
int evaluate_flags(int flag1, int flag2);

#endif /* FILE_HANDLER_H */
