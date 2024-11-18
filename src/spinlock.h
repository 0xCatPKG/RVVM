/*
spinlock.h - Fast hybrid reader/writer lock
Copyright (C) 2021  LekKit <github.com/LekKit>

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

#ifndef RVVM_SPINLOCK_H
#define RVVM_SPINLOCK_H

#include <stddef.h>
#include "compiler.h"
#include "atomics.h"

/*
 * Lock flags meaning:
 * 0x00000001: Writer holds the lock
 * 0x7FFFFFFE: Reader count
 * 0x80000000: There are waiters
 */

// Static initialization
#define SPINLOCK_INIT {0}

#ifdef USE_SPINLOCK_DEBUG
#define SPINLOCK_DEBUG_LOCATION SOURCE_LINE
#else
#define SPINLOCK_DEBUG_LOCATION NULL
#endif

typedef struct {
    uint32_t flag;
#ifdef USE_SPINLOCK_DEBUG
    const char* location;
#endif
} spinlock_t;

// Slow path for kernel wait/wake on contended lock
slow_path void spin_lock_wait(spinlock_t* lock, const char* location);
slow_path void spin_lock_wake(spinlock_t* lock, uint32_t prev);

slow_path void spin_read_lock_wait(spinlock_t* lock, const char* location);
slow_path void spin_read_lock_wake(spinlock_t* lock, uint32_t prev);

// Initialize a lock
static inline void spin_init(spinlock_t* lock)
{
    atomic_store_uint32(&lock->flag, 0);
#ifdef USE_SPINLOCK_DEBUG
    lock->location = NULL;
#endif
}

/*
 * Writer locking
 */

static forceinline bool spin_try_lock_internal(spinlock_t* lock, const char* location)
{
    bool ret = atomic_cas_uint32_ex(&lock->flag, 0x0, 0x1, false, ATOMIC_ACQUIRE, ATOMIC_RELAXED);
#ifdef USE_SPINLOCK_DEBUG
    if (likely(ret)) lock->location = location;
#else
    UNUSED(location);
#endif
    return ret;
}

static forceinline void spin_lock_internal(spinlock_t* lock, const char* location)
{
    if (unlikely(!spin_try_lock_internal(lock, location))) {
        spin_lock_wait(lock, location);
    }
}

static forceinline void spin_lock_slow_internal(spinlock_t* lock, const char* location)
{
    if (unlikely(!spin_try_lock_internal(lock, location))) {
        spin_lock_wait(lock, NULL);
    }
}

// Try to claim the writer lock
#define spin_try_lock(lock)  spin_try_lock_internal(lock, SPINLOCK_DEBUG_LOCATION)

// Perform writer locking on small, bounded critical section
// Reports a deadlock upon waiting for too long
#define spin_lock(lock)      spin_lock_internal(lock, SPINLOCK_DEBUG_LOCATION)

// Perform writer locking around heavy operation, wait indefinitely
#define spin_lock_slow(lock) spin_lock_slow_internal(lock, SPINLOCK_DEBUG_LOCATION)

// Release the writer lock
static forceinline void spin_unlock(spinlock_t* lock)
{
    uint32_t prev = atomic_swap_uint32_ex(&lock->flag, 0x0, ATOMIC_RELEASE);
    if (unlikely(((int32_t)prev) <= 0)) {
        spin_lock_wake(lock, prev);
    }
}

/*
 * Reader locking
 */

// Try to claim the reader lock
static forceinline bool spin_try_read_lock(spinlock_t* lock)
{
    uint32_t prev = 0;
    do {
        prev = atomic_load_uint32_ex(&lock->flag, ATOMIC_RELAXED);
        if (unlikely((prev & 0x1) || ((int32_t)prev) < 0)) {
            // Writer owns the lock, writer waiters are present or too much readers (sic!)
            return false;
        }
    } while (unlikely(!atomic_cas_uint32_ex(&lock->flag, prev, prev + 2, true, ATOMIC_ACQUIRE, ATOMIC_RELAXED)));
    return true;
}

static forceinline void spin_read_lock_internal(spinlock_t* lock, const char* location)
{
    if (unlikely(!spin_try_read_lock(lock))) {
        spin_read_lock_wait(lock, location);
    }
}

// Perform reader locking on small, bounded critical section
// Reports a deadlock upon waiting for too long
#define spin_read_lock(lock)      spin_read_lock_internal(lock, SPINLOCK_DEBUG_LOCATION)

// Perform reader locking around heavy operation, wait indefinitely
#define spin_read_lock_slow(lock) spin_read_lock_internal(lock, NULL)

// Release the reader lock
static forceinline void spin_read_unlock(spinlock_t* lock)
{
    uint32_t prev = atomic_sub_uint32_ex(&lock->flag, 0x2, ATOMIC_RELEASE);
    if (unlikely(((int32_t)prev) <= 0)) {
        spin_read_lock_wake(lock, prev);
    }
}

#endif
