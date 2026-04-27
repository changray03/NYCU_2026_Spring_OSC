#ifndef TASK_H
#define TASK_H

#include <stddef.h>
#include <stdint.h>

typedef void (*task_callback_t)(void *arg);
typedef struct task_struct {
    int priority;
    task_callback_t callback;
    void *arg;
    struct task_struct *next;
} task_t;

extern int current_task_priority;

void add_task(task_callback_t callback, void *arg, int priority);
void run_preemptive_tasks();

#endif