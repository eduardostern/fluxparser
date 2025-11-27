/*
 * Thread Pool for Parallel Attention Heads
 * Eliminates pthread creation/destruction overhead
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdbool.h>

/* Task function type */
typedef void* (*ThreadPoolFunc)(void*);

/* Thread pool structure */
typedef struct ThreadPool ThreadPool;

/* Create thread pool with specified number of worker threads */
ThreadPool* thread_pool_create(int num_threads);

/* Submit a batch of tasks to the pool and wait for completion */
void thread_pool_execute(ThreadPool *pool, ThreadPoolFunc func, void **args, int num_tasks);

/* Destroy thread pool and wait for workers to finish */
void thread_pool_destroy(ThreadPool *pool);

/* Get number of worker threads in pool */
int thread_pool_size(ThreadPool *pool);

#endif /* THREAD_POOL_H */
