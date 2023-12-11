#include "thread_pool.h"
#include<stdlib.h>
#include<stdio.h>
#include <pthread.h>

enum task_status {
    CREATED,
    QUEUED,
    RUNNING,
    DONE,
    JOINED
};

struct thread_task {
	thread_task_f function;
	void *arg;
	void *result;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
	enum task_status status;
};


struct thread_pool {
	pthread_t *threads;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int max_threads_number;
	int created_threads_number;
	int number_thread_busy;
	int task_waiting;
	bool is_deleted;
	struct thread_task *task_queue[TPOOL_MAX_TASKS];
	int task_queue_write_pos;
	int task_queue_read_pos;
};

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
	if (max_thread_count <= 0 || max_thread_count > TPOOL_MAX_THREADS) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}
	struct thread_pool *new_pool = (struct thread_pool*)malloc(sizeof(struct thread_pool));
	new_pool->max_threads_number = max_thread_count;
	new_pool->task_waiting = 0;
	new_pool->number_thread_busy = 0;
	new_pool->is_deleted = false;
	new_pool->threads = (pthread_t*)calloc(max_thread_count, sizeof(pthread_t));
    new_pool->created_threads_number = 0;
	new_pool->task_queue_write_pos = 0;
	new_pool->task_queue_read_pos = 0;
	pthread_mutex_init(&new_pool->mutex, NULL);
	pthread_cond_init(&new_pool->cond, NULL);

	*pool = new_pool;
	return 0;
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	return pool->created_threads_number;
}

int
thread_pool_delete(struct thread_pool *pool)
{
	pthread_mutex_lock(&pool->mutex);
    if (pool->number_thread_busy > 0 || pool->task_waiting > 0) {
        pthread_mutex_unlock(&pool->mutex);
        return TPOOL_ERR_HAS_TASKS;
    }
	pool->is_deleted = true;
	// Wake up all threads to delete them
	pthread_cond_broadcast(&pool->cond);
	pthread_mutex_unlock(&pool->mutex);

	for (int i = 0; i < pool->created_threads_number; i++) {
		pthread_join(pool->threads[i], NULL);
	}

	free(pool->threads);
	pthread_mutex_destroy(&pool->mutex);
	pthread_cond_destroy(&pool->cond);
	free(pool);
	return 0;
}

void* start_thread_routine_func(void* arg) {
	struct thread_pool *pool = (struct thread_pool *)arg;
	while(1) {
		pthread_mutex_lock(&pool->mutex);

		// Wait for new tasks if there is not in queue
		if (pool->task_waiting == 0 && pool->is_deleted == false) {
			pthread_cond_wait(&pool->cond, &pool->mutex);
		}
		
        if (pool->is_deleted) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

		struct thread_task* task = pool->task_queue[pool->task_queue_read_pos];
		pool->task_queue[pool->task_queue_read_pos] = NULL;
		pool->task_queue_read_pos++;
		pool->number_thread_busy++;
		pool->task_waiting--;

		if (pool->task_queue_read_pos == TPOOL_MAX_TASKS) {
            pool->task_queue_read_pos = 0;
        }

		// Here we sure that nobody already took this task due to we lock mutex on pool.
		// So now we can lock task mutex and unlock pool mutex.
		pthread_mutex_lock(&task->mutex);
		pthread_mutex_unlock(&pool->mutex);

		task->status = RUNNING;
		pthread_mutex_unlock(&task->mutex);
		task->result = task->function(task->arg);

		pthread_mutex_lock(&pool->mutex);
		pool->number_thread_busy--;
		pthread_mutex_unlock(&pool->mutex);

		pthread_mutex_lock(&task->mutex);
		task->status = DONE;
		// Unlock everybody who waiting this task
		pthread_cond_broadcast(&task->cond);
		pthread_mutex_unlock(&task->mutex);
	}
	return 0;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
	printf("%p\n", &pool->mutex);
	printf("qwe\n");
	pthread_mutex_lock(&pool->mutex);
	if (pool->task_waiting + pool->number_thread_busy >= TPOOL_MAX_TASKS) {
        pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}
	pthread_mutex_lock(&task->mutex);
	printf("qwe\n");
	
	printf("got mutexs\n");
	task->status = QUEUED;
	pthread_mutex_unlock(&task->mutex);
	
	pool->task_queue[pool->task_queue_write_pos] = task;
	pool->task_waiting++;
	pool->task_queue_write_pos++;
	if (pool->task_queue_write_pos == TPOOL_MAX_TASKS) {
		pool->task_queue_write_pos = 0;
	}

	if (pool->number_thread_busy < pool->created_threads_number || pool->created_threads_number >= pool->max_threads_number) {
		pthread_cond_signal(&pool->cond);
	} else {
		pthread_create(&pool->threads[pool->created_threads_number], NULL, start_thread_routine_func, pool);
		pool->created_threads_number++;
	}
	pthread_mutex_unlock(&pool->mutex);
	
	return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	struct thread_task* new_task = (struct thread_task*)malloc(sizeof(struct thread_task));
	new_task->function = function;
	new_task->arg = arg;

    pthread_mutex_init(&new_task->mutex, NULL);
    pthread_cond_init(&new_task->cond, NULL);

    *task = new_task;
	return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	return task->status == DONE && task->status == JOINED;
}

bool
thread_task_is_running(const struct thread_task *task)
{
	return task->status == RUNNING;
}

int
thread_task_join(struct thread_task *task, void **result)
{
	pthread_mutex_lock(&task->mutex);
	if (task->status == CREATED) {
        pthread_mutex_unlock(&task->mutex);
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}
	while (task->status != DONE && task->status != JOINED) {
		printf("wait\n");
		pthread_cond_wait(&task->cond, &task->mutex);
	}
	printf("uhu\n");
	*result = task->result;
    task->status = JOINED;
	pthread_mutex_unlock(&task->mutex);
	return 0;
}

#ifdef NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)timeout;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
    pthread_mutex_lock(&task->mutex);

    if (task->status == CREATED || task->status == JOINED) {
        pthread_mutex_unlock(&task->mutex);
        pthread_mutex_destroy(&task->mutex);
        pthread_cond_destroy(&task->cond);
        free(task);
        return 0;
    }

    pthread_mutex_unlock(&task->mutex);
    return TPOOL_ERR_TASK_IN_POOL;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
