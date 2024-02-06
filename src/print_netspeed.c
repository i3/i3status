#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "i3status.h"

#define NET_STAT_BUFFER_SIZE 100
#define RATE_BUFFER_SIZE 10

typedef struct {
    unsigned long last_time;
    unsigned long last_rx;
    unsigned long last_tx;
    char last_rate_down[RATE_BUFFER_SIZE];
    char last_rate_up[RATE_BUFFER_SIZE];
} NetworkState;

static NetworkState prev_state = {
    0,
    0,
    0,
    {'0', '\0'},
    {'0', '\0'}};

void readable(unsigned long bytes, char *output) {
    unsigned long kb = bytes / 1000;  // Convert to kilobytes (base-10)
    if (kb < 1000) {
        sprintf(output, "%lu KB", kb);
    } else {
        unsigned long mb_int = kb / 1000;
        unsigned long mb_dec = (kb % 1000 * 100) / 1000;  // Multiplied by 100, then divided by 1000 for 2 decimal places
        sprintf(output, "%lu.%02lu MB", mb_int, mb_dec);
    }
}

void print_netspeed(netspeed_ctx_t *ctx) {
    char *outwalk = ctx->buf;
    const char *selected_format = ctx->format;

    unsigned long current_time = (unsigned long)time(NULL);

    DIR *d;
    d = opendir("/sys/class/net");
    if (d == NULL)
        return;

    int ret;
    char path[PATH_MAX], line[PATH_MAX];
    char interface[PATH_MAX];
    char resolved_path[PATH_MAX];
    struct dirent *dir;
    unsigned long rx = 0, tx = 0;

    const char virtual_iface_path[] = "/sys/devices/virtual/net/";
    size_t virtual_iface_len = strlen(virtual_iface_path);

    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
            continue;

        snprintf(interface, sizeof(interface), "/sys/class/net/%s", dir->d_name);

        if (realpath(interface, resolved_path) == NULL)
            continue;

        size_t full_path_len = virtual_iface_len + strlen(dir->d_name) + 1;
        snprintf(path, full_path_len, "%s%s", virtual_iface_path, dir->d_name);

         // check if interface is virtual
        if (strcmp(path, resolved_path) == 0)
            continue;

        ret = snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", dir->d_name);
        if (ret >= sizeof(path) || ret < 0)
            continue;

        FILE *fp = fopen(path, "r");
        fgets(line, NET_STAT_BUFFER_SIZE, fp);
        fclose(fp);
        rx += strtoul(line, NULL, 10);

        ret = snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", dir->d_name);
        if (ret >= sizeof(path) || ret < 0)
            continue;

        fp = fopen(path, "r");
        fgets(line, NET_STAT_BUFFER_SIZE, fp);
        fclose(fp);
        tx += strtoul(line, NULL, 10);
    }
    closedir(d);

    unsigned long interval = current_time - prev_state.last_time;
    if (interval > 0) {
        char rate_down[RATE_BUFFER_SIZE], rate_up[RATE_BUFFER_SIZE];

        readable((rx - prev_state.last_rx) / interval, rate_down);
        readable((tx - prev_state.last_tx) / interval, rate_up);

        strncpy(prev_state.last_rate_down, rate_down, RATE_BUFFER_SIZE);
        strncpy(prev_state.last_rate_up, rate_up, RATE_BUFFER_SIZE);

        prev_state.last_time = current_time;
        prev_state.last_rx = rx;
        prev_state.last_tx = tx;
    }

    placeholder_t placeholders[] = {
        {.name = "%down", .value = prev_state.last_rate_down},
        {.name = "%up", .value = prev_state.last_rate_up}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    char *formatted = format_placeholders(selected_format, &placeholders[0], num);
    OUTPUT_FORMATTED;
    free(formatted);
    OUTPUT_FULL_TEXT(ctx->buf);
}
