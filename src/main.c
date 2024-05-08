#include <stdio.h>
#include <stdlib.h>

#include "file_processor.h"
#include "logger.h"
#include "loggerconf.h"
#include "plugin_api.h"

struct option *longopts = NULL;
struct plugin_list plug_list = {NULL};

int main(int argc, char *argv[]) {
    /* init logger */
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

    /* get plugin path */
    char *path = get_plugin_path(argc, argv);
    LOG_DEBUG("Plugin path: %s", path);

    /* load plugins */
    load_plugins(path, &plug_list, &longopts);
    path = parse_options(argc, argv, longopts, &plug_list);
    LOG_DEBUG("Search path: %s", path);
    free(longopts);
    
    /* filter plugins */
    filter_plugins(&plug_list);

    /* run plugins */
    process_directory_files(path, &plug_list);

    /* free plugins */
    clear_plugins(&plug_list);
    free(path);

    return 0;
} 