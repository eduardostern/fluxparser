/*
 * Thread Pool Implementation
 * Reuses worker threads to eliminate pthread creation overhead
 */

#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

/* Thread pool internal structure */
struct ThreadPool {
    pthread_t *workers;           /* Worker thread IDs */
    int num_threads;              /* Number of worker threads */

    /* Task queue */
    ThreadPoolFunc func;          /* Function to execute */
    void **args;                  /* Array of arguments for each task */
    int num_tasks;                /* Number of tasks to execute */
    int next_task;                /* Next task index to execute */
    int completed_tasks;          /* Number of completed tasks */

    /* Synchronization */
    pthread_mutex_t mutex;        /* Protects task queue */
    pthread_cond_t work_available; /* Signals work is available */
    pthread_cond_t work_done;     /* Signals all work is done */

    bool shutdown;                /* Thread pool is shutting down */
};

/* Worker thread function */
static void* worker_thread(void *arg) {
    ThreadPool *pool = (ThreadPool*)arg;

    while (1) {
        pthread_mutex_lock(&pool->mutex);

        /* Wait for work or shutdown signal */
        while (pool->next_task >= pool->num_tasks && !pool->shutdown) {
            pthread_cond_wait(&pool->work_available, &pool->mutex);
        }

        /* Check if we should shutdown */
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        /* Get next task */
        int task_id = pool->next_task++;
        pthread_mutex_unlock(&pool->mutex);

        /* Execute task (outside lock for parallelism) */
        if (task_id < pool->num_tasks) {
            pool->func(pool->args[task_id]);

            /* Mark task as completed */
            pthread_mutex_lock(&pool->mutex);
            pool->completed_tasks++;

            /* Signal if all tasks are done */
            if (pool->completed_tasks == pool->num_tasks) {
                pthread_cond_signal(&pool->work_done);
            }
            pthread_mutex_unlock(&pool->mutex);
        }
    }

    return NULL;
}

/* Create thread pool */
ThreadPool* thread_pool_create(int num_threads) {
    if (num_threads <= 0) {
        num_threads = 1;
    }

    ThreadPool *pool = malloc(sizeof(ThreadPool));
    if (!pool) return NULL;

    pool->num_threads = num_threads;
    pool->workers = malloc(sizeof(pthread_t) * num_threads);
    if (!pool->workers) {
        free(pool);
        return NULL;
    }

    /* Initialize synchronization primitives */
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->work_available, NULL);
    pthread_cond_init(&pool->work_done, NULL);

    /* Initialize task queue as empty */
    pool->func = NULL;
    pool->args = NULL;
    pool->num_tasks = 0;
    pool->next_task = 0;
    pool->completed_tasks = 0;
    pool->shutdown = false;

    /* Create worker threads */
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->workers[i], NULL, worker_thread, pool) != 0) {
            /* Failed to create thread, cleanup and return NULL */
            pool->shutdown = true;
            pthread_cond_broadcast(&pool->work_available);

            for (int j = 0; j < i; j++) {
                pthread_join(pool->workers[j], NULL);
            }

            pthread_mutex_destroy(&pool->mutex);
            pthread_cond_destroy(&pool->work_available);
            pthread_cond_destroy(&pool->work_done);
            free(pool->workers);
            free(pool);
            return NULL;
        }
    }

    return pool;
}

/* Execute tasks in parallel using thread pool */
void thread_pool_execute(ThreadPool *pool, ThreadPoolFunc func, void **args, int num_tasks) {
    if (!pool || !func || !args || num_tasks <= 0) {
        return;
    }

    pthread_mutex_lock(&pool->mutex);

    /* Set up task queue */
    pool->func = func;
    pool->args = args;
    pool->num_tasks = num_tasks;
    pool->next_task = 0;
    pool->completed_tasks = 0;

    /* Wake up all worker threads */
    pthread_cond_broadcast(&pool->work_available);

    /* Wait for all tasks to complete */
    while (pool->completed_tasks < pool->num_tasks) {
        pthread_cond_wait(&pool->work_done, &pool->mutex);
    }

    pthread_mutex_unlock(&pool->mutex);
}

/* Destroy thread pool */
void thread_pool_destroy(ThreadPool *pool) {
    if (!pool) return;

    /* Signal shutdown */
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->work_available);
    pthread_mutex_unlock(&pool->mutex);

    /* Wait for all worker threads to finish */
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->workers[i], NULL);
    }

    /* Cleanup */
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->work_available);
    pthread_cond_destroy(&pool->work_done);
    free(pool->workers);
    free(pool);
}

/* Get pool size */
int thread_pool_size(ThreadPool *pool) {
    return pool ? pool->num_threads : 0;
}
