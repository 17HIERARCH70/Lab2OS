#include <stdio.h>
#include <stdlib.h>
#include "file_handler.h"
#include "logger.h"
#include "loggerconf.h"
#include "plugin_api.h"

struct option *long_options = NULL;
struct plugin_list plugins = {NULL};

void initialize_logger() {
    char *debug_env = getenv("LAB1DEBUG");
    int debug_level = debug_env ? atoi(debug_env) : 0;

    if (debug_level >= 1) {
        logger_initConsoleLogger(stderr);
        logger_setLevel(LogLevel_DEBUG);
        logger_initFileLogger("logs/log.txt", 1024 * 1024, 5);
        logger_autoFlush(0);
        LOG_INFO("Debug mode is on");
    } else {
        logger_initConsoleLogger(stdout);
        logger_setLevel(LogLevel_INFO);
        logger_autoFlush(0);
        LOG_INFO("Debug mode is off");
    }
}

void execute_pipeline(int argc, char *argv[]) {
    char *plugin_path = get_plugin_directory_path(argc, argv);
    LOG_DEBUG("Plugin path: %s", plugin_path);

    load_plugins_from_directory(plugin_path, &plugins, &long_options);
    free(plugin_path);

    char *search_path = parse_command_line_arguments(argc, argv, long_options, &plugins);
    LOG_DEBUG("Search path: %s", search_path);
    
    filter_active_plugins(&plugins);

    handle_directory_files(search_path, &plugins);

    free(search_path);
}

void clean_up() {
    free(long_options);
    clear_plugin_list(&plugins);
}

int main(int argc, char *argv[]) {
    initialize_logger();
    execute_pipeline(argc, argv);
    clean_up();
    return 0;
}
