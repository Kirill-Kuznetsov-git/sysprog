#include<stdlib.h>
#include<stdio.h>
#include <pthread.h>
#include <time.h>

void* start_thread_routine_func(void* arg) {
    return NULL;
}

long double benchmark() {
    struct timespec start_time;
    struct timespec end_time;
    pthread_t thread;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    pthread_create(&thread, NULL, start_thread_routine_func, NULL);
    pthread_join(thread, NULL);
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    return (double)(end_time.tv_nsec - start_time.tv_nsec) / 1000000;
}

int main() {
    
    int n = 100000;
    int good_measures = 0;
    long double measures_sum = 0;
    long double measure = 0;
    while (good_measures != n) {
        measure = benchmark();
        if (measure <= 0) {
            continue;
        }
        good_measures++;
        measures_sum += measure;
    }
    
    printf("In average thread create+join over %d interation take %Lf msec.\n", good_measures, (long double)measures_sum / good_measures);
}
