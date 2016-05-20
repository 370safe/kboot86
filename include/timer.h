#pragma once

#include <stdbool.h>

typedef struct st_timer {
    unsigned int            init_count;

    volatile unsigned int   counter;

    struct st_timer         *prev;
    struct st_timer         *next;
} timer_t;


void timer_start(timer_t *, unsigned int);
void timer_end(timer_t *);
void timer_restart(timer_t *);
bool timer_is_triggered(const timer_t *);