#define _POSIX_C_SOURCE 200809L
#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// Talk to Hyprland's IPC socket directly (no fork/exec).
// Path is for Hyprland >= 0.40; older versions used /tmp/hypr/<sig>/.
static char *hypr_request(const char *request) {
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!runtime || !sig)
        return NULL;

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/hypr/%s/.socket.sock",
             runtime, sig);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return NULL;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        write(fd, request, strlen(request)) < 0) {
        close(fd);
        return NULL;
    }

    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    ssize_t n;
    while (buf && (n = read(fd, buf + len, cap - len - 1)) > 0) {
        len += (size_t)n;
        if (len + 1 >= cap) {
            char *bigger = realloc(buf, cap *= 2);
            if (!bigger) {
                free(buf);
                buf = NULL;
                break;
            }
            buf = bigger;
        }
    }
    close(fd);
    if (buf)
        buf[len] = '\0';
    return buf;
}

static const char *json_value(const char *obj, const char *key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(obj, pattern);
    if (!p)
        return NULL;
    p += strlen(pattern);
    while (*p == ':' || *p == ' ')
        p++;
    return p;
}

bool get_focused_monitor(Monitor *out) {
    char *reply = hypr_request("j/monitors");
    if (!reply)
        return false;

    bool found = false, in_string = false;
    int depth = 0;
    char *start = NULL;

    for (char *p = reply; *p && !found; p++) {
        if (in_string) {
            if (*p == '\\' && p[1])
                p++;
            else if (*p == '"')
                in_string = false;
        } else if (*p == '"') {
            in_string = true;
        } else if (*p == '{') {
            if (depth++ == 0)
                start = p;
        } else if (*p == '}' && --depth == 0) {
            char saved = p[1];
            p[1] = '\0'; 

            const char *focused = json_value(start, "focused");
            if (focused && strncmp(focused, "true", 4) == 0) {
                const char *name = json_value(start, "name");
                const char *w = json_value(start, "width");
                const char *h = json_value(start, "height");
                const char *x = json_value(start, "x");
                const char *y = json_value(start, "y");
                const char *s = json_value(start, "scale");

                out->name[0] = '\0';
                if (name && *name == '"')
                    sscanf(name + 1, "%63[^\"]", out->name);
                out->width = w ? atoi(w) : 0;
                out->height = h ? atoi(h) : 0;
                out->x = x ? atoi(x) : 0;
                out->y = y ? atoi(y) : 0;
                out->scale = s ? strtof(s, NULL) : 1.0f;
                if (out->scale <= 0.0f)
                    out->scale = 1.0f;

                found = out->name[0] && out->width > 0 && out->height > 0;
            }
            p[1] = saved;
        }
    }
    free(reply);
    return found;
}