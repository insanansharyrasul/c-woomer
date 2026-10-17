#ifndef MONITOR_H
#define MONITOR_H

#include <stdbool.h>

typedef struct {
    char name[64];
    int x, y;          
    int width, height; 
    float scale;
} Monitor;

bool get_focused_monitor(Monitor *out);

#endif