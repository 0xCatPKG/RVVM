/*
threading.h - Threading, Conditional variables, Task offloading
Copyright (C) 2021  LekKit <github.com/LekKit>

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

#ifndef THREADING_H
#define THREADING_H

#include <stdint.h>
#include <stdbool.h>

typedef struct thread_ctx thread_ctx_t;
typedef struct cond_var cond_var_t;

typedef void* (*thread_func_t)(void*);
typedef void* (*thread_func_va_t)(void**);

// Threading
thread_ctx_t* thread_create_ex(thread_func_t func, void* arg, uint32_t stack_size);
thread_ctx_t* thread_create(thread_func_t func, void* arg);
void*         thread_join(thread_ctx_t* handle);
bool          thread_detach(thread_ctx_t* handle);

#define CONDVAR_INFINITE ((uint64_t)-1)

// Conditional variables
cond_var_t* condvar_create(void);
bool        condvar_wait(cond_var_t* cond, uint64_t timeout_ms);
bool        condvar_wait_ns(cond_var_t* cond, uint64_t timeout_ns);
bool        condvar_wake(cond_var_t* cond);
bool        condvar_wake_all(cond_var_t* cond);
uint32_t    condvar_waiters(cond_var_t* cond);
void        condvar_free(cond_var_t* cond);

#define THREAD_MAX_VA_ARGS 8

// Execute task in threadpool
void thread_create_task(thread_func_t func, void* arg);
void thread_create_task_va(thread_func_va_t func, void** args, unsigned arg_count);

#endif
