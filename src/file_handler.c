#include "file_handler.h"
#include "logger.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Function to evaluate flags based on the 'option_O' flag
int evaluate_flags(int flag1, int flag2) {
    return option_O ? (flag1 && flag2) : (flag1 || flag2);
}

// Function to check a file against the plugins in the list
int process_file_with_plugins(char *filename, struct plugin_list *plugins) {
    LOG_DEBUG("process_file_with_plugins: Processing file: %s", filename);

    struct plugin_list_node *current_plugin = plugins->head;
    int combined_flag = option_O;
    int plugin_result;

    while (current_plugin) {
        plugin_result = (*(current_plugin->plugin.func))(filename, 
                            current_plugin->plugin.opts,
                            current_plugin->plugin.opts_len);
        
        if (plugin_result == -1) {
            LOG_ERROR("process_file_with_plugins: Error in plugin while processing file: %s",
                    filename);
            return -1;
        }

        combined_flag = evaluate_flags(combined_flag, plugin_result);
        
        if (combined_flag && option_A) {
            break;
        }
        
        current_plugin = current_plugin->next;
    }

    return !combined_flag;
}

// Function to recursively process files in a directory
void handle_directory_files(char *directory_path, struct plugin_list *plugins) {
    LOG_DEBUG("handle_directory_files: Processing directory: %s", directory_path);

    struct dirent *file_entry;
    struct stat file_stat;
    DIR *directory = opendir(directory_path);
    int plugin_result = 0;

    if (!directory) {
        LOG_ERROR("handle_directory_files: Error opening directory: %s", directory_path);
        return;
    }

    while ((file_entry = readdir(directory)) != NULL) {
        char *file_path = (char *)malloc(strlen(directory_path) + strlen(file_entry->d_name) + 2);
        snprintf(file_path, strlen(directory_path) + strlen(file_entry->d_name) + 2, "%s/%s",
                 directory_path, file_entry->d_name);

        if (strcmp(file_entry->d_name, ".") == 0 || strcmp(file_entry->d_name, "..") == 0) {
            free(file_path);
            continue;
        }

        lstat(file_path, &file_stat);

        if ((!S_ISREG(file_stat.st_mode) && !S_ISDIR(file_stat.st_mode)) ||
            file_stat.st_size == 0) {
            free(file_path);
            continue;
        }

        if ((file_stat.st_mode & S_IFMT) == S_IFDIR) {
            handle_directory_files(file_path, plugins);
        } else {
            plugin_result = process_file_with_plugins(file_path, plugins);
            if (plugin_result == -1) {
                free(file_path);
                closedir(directory);
                exit(EXIT_FAILURE);
            }
            if (plugin_result) {
                LOG_INFO("%s\n", file_path);
            }
        }
        free(file_path);
    }
    closedir(directory);
}
