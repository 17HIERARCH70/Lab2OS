#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>

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

static char *g_lib_name = "libipv4.so";
static struct plugin_option g_pi[] = {
    {{"ipv4-addr-bin", required_argument, NULL, 0}, "Поиск файлов, содержащих заданный IPv4-адрес в бинарной форме"},
    {{NULL, 0, NULL, 0}, NULL} // Terminate the array
};

int plugin_get_info(struct plugin_info *ppi) {
    ppi->plugin_purpose = "Поиск файлов, содержащих заданный IPv4-адрес в бинарной форме";
    ppi->plugin_author = "Ваше Имя, NXXXX";
    ppi->sup_opts_len = 1;
    ppi->sup_opts = g_pi;
    return 0;
}

int parse_ipv4_address(const char *str, uint32_t *addr) {
    struct in_addr ipv4;
    if (inet_pton(AF_INET, str, &ipv4) != 1) {
        return 0;
    }
    *addr = ipv4.s_addr;
    return 1;
}

int plugin_process_file(const char *fname, struct option in_opts[], size_t in_opts_len) {
    char *DEBUG = getenv("LAB1DEBUG");
    if (DEBUG) {
        fprintf(stderr, "DEBUG: %s: Checking file '%s'\n", g_lib_name, fname);
        for (size_t i = 0; i < in_opts_len; i++) {
            fprintf(stderr, "DEBUG: %s: Got option '%s' with arg '%s'\n", g_lib_name, in_opts[i].name, (char *)in_opts[i].flag);
        }
    }

    if (in_opts_len != 1 || strcmp(in_opts[0].name, "ipv4-addr-bin") != 0) {
        fprintf(stdout, "Опция ipv4-addr-bin требует аргумент\n");
        fprintf(stderr, "DEBUG: %s: Invalid or missing argument for 'ipv4-addr-bin'\n", g_lib_name);
        return -1;
    }

    uint32_t target_ip;
    if (!parse_ipv4_address((char *)in_opts[0].flag, &target_ip)) {
        fprintf(stdout, "Неверный аргумент опции ipv4-addr-bin\n");
        if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: Invalid IPv4 address argument for 'ipv4-addr-bin'\n", g_lib_name);
        }
        return -1;
    }

    int fd = open(fname, O_RDONLY);
    if (fd == -1) {
        if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: Failed to open file '%s'\n", g_lib_name, fname);
        }
        return 1;
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: Failed to get file stats for '%s'\n", g_lib_name, fname);
        }
        close(fd);
        return 1;
    }

    if (file_stat.st_size < 4) {
        if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: File '%s' is too small to contain an IPv4 address\n", g_lib_name, fname);
        }
        close(fd);
        return 1;
    }

    char *data = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: mmap failed for file '%s'\n", g_lib_name, fname);
        }
        close(fd);
        return 1;
    }

    int found = 0;
    for (off_t i = 0; i <= file_stat.st_size - 4; i++) {
        uint32_t *addr = (uint32_t *)(data + i);
        if (*addr == target_ip || *addr == __builtin_bswap32(target_ip)) {
            found = 1;
            break;
        }
    }

    munmap(data, file_stat.st_size);
    close(fd);

    return found ? 0 : 1;
}
