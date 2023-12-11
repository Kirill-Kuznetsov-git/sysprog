#include<stdlib.h>
#include<stdio.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

enum Mode {
    SIGNAL,
    BROADCAST
};

struct thread_task {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    long result_nsec;
    int wait_threads_gotted_mutex;
    int n_signals;
};


void* signal_thread_routine_func(void* arg) {
    struct timespec start_time;
    struct timespec end_time;
    struct thread_task *thread_task = (struct thread_task *)arg;
    pthread_mutex_lock(&thread_task->mutex);
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (int i = 0; i < thread_task->n_signals; i++) {
        pthread_cond_signal(&thread_task->cond);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    pthread_mutex_unlock(&thread_task->mutex);
    thread_task->result_nsec = (end_time.tv_nsec - start_time.tv_nsec) + (end_time.tv_sec - start_time.tv_sec) * 1000000000;
    return NULL;
}

void* broadcast_thread_routine_func(void* arg) {
    struct timespec start_time;
    struct timespec end_time;
    struct thread_task *thread_task = (struct thread_task *)arg;
    pthread_mutex_lock(&thread_task->mutex);
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (int i = 0; i < thread_task->n_signals; i++) {
        pthread_cond_broadcast(&thread_task->cond);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    pthread_mutex_unlock(&thread_task->mutex);
    thread_task->result_nsec = (end_time.tv_nsec - start_time.tv_nsec) + (end_time.tv_sec - start_time.tv_sec) * 1000000000;
    return NULL;
}

void* wait_thread_routine_func(void* arg) {
    struct timeval now;
    struct timespec timeout;
    gettimeofday(&now, 0);
    timeout.tv_sec = now.tv_sec + 1;

    struct thread_task *thread_task = (struct thread_task *)arg;
    pthread_mutex_lock(&thread_task->mutex);
    thread_task->wait_threads_gotted_mutex++;
    while (1) {
        int ret = pthread_cond_timedwait(&thread_task->cond, &thread_task->mutex, &timeout);
        if (ret == ETIMEDOUT) {
            break;
        }
    }
    pthread_mutex_unlock(&thread_task->mutex);
    return NULL;
}

long benchmark(int number_wait_threads, struct thread_task* thread_task, enum Mode mode) {
    pthread_t signal_thread;
    pthread_t* wait_threads = (pthread_t*)malloc(sizeof(pthread_t) * number_wait_threads);
    
    for (int i = 0; i < number_wait_threads; i++) {
        pthread_create(&wait_threads[i], NULL, wait_thread_routine_func, thread_task);
    }
    while (thread_task->wait_threads_gotted_mutex != number_wait_threads) {}
    if (mode == SIGNAL) {
        pthread_create(&signal_thread, NULL,  signal_thread_routine_func, thread_task);
    } else {
        pthread_create(&signal_thread, NULL,  broadcast_thread_routine_func, thread_task);
    }
    for (int i = 0; i < number_wait_threads; i++) {
        pthread_join(wait_threads[i], NULL);
    }
    pthread_join(signal_thread, NULL);

    return thread_task->result_nsec;
}

void printBenchmarkResult(int number_wait_pthreads, enum Mode mode, struct thread_task* new_task) {
    long measure = benchmark(number_wait_pthreads, new_task, mode);
    printf("%d %s with %d wait threads take %Lf msec.\n",
    new_task->n_signals, mode == BROADCAST ? "broadcast" : "signal", number_wait_pthreads, (long double)measure / 1000000);
}

int main() {
    struct thread_task* new_task = (struct thread_task*)malloc(sizeof(struct thread_task));
    new_task->n_signals = 1000000;
    new_task->wait_threads_gotted_mutex = 0;
    new_task->result_nsec = 0;
    pthread_mutex_init(&new_task->mutex, NULL);
    pthread_cond_init(&new_task->cond, NULL);

    printBenchmarkResult(1, SIGNAL, new_task);
    new_task->wait_threads_gotted_mutex = 0;
    new_task->result_nsec = 0;
    printBenchmarkResult(1, BROADCAST, new_task);
    new_task->wait_threads_gotted_mutex = 0;
    new_task->result_nsec = 0;
    printBenchmarkResult(3, SIGNAL, new_task);
    new_task->wait_threads_gotted_mutex = 0;
    new_task->result_nsec = 0;
    printBenchmarkResult(3, BROADCAST, new_task);
}