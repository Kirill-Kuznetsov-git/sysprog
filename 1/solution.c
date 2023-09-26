#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libcoro.h"

typedef struct sorted_arr sorted_arr;
/**
 * You can compile and run this code using the commands:
 *
 * $> gcc solution.c libcoro.c
 * $> ./a.out
 */

struct sorted_arr {
  int* arr;
  int arr_size;
};

struct my_context {
	char *name;
    char *file_name;
    int* numbers;
    int numbers_size;
    long work_time_nanosec;
    struct timespec last_froze;
};

void printArr(int* arr, int arr_size, char divider, char *coro_name) {
    printf("%c%c%c%s\n", divider, divider, divider, coro_name);
    for (int i = 0; i < arr_size; i++) {
      printf("%d ", arr[i]);
    }
    printf("\n");
    printf("%c%c%c\n", divider, divider, divider);
}

static struct my_context *
my_context_new(const char *name, const char *file_name, int work_time_nanosec)
{
	struct my_context *ctx = malloc(sizeof(*ctx));
	ctx->name = strdup(name);
    ctx->file_name = strdup(file_name);
    ctx->work_time_nanosec = work_time_nanosec;
    clock_gettime(CLOCK_MONOTONIC, &ctx->last_froze);
	return ctx;
}

static void
my_context_delete(struct my_context *ctx)
{
	free(ctx->name);
    free(ctx->file_name);
	free(ctx);
}

void quick_sort(int first, int last, struct my_context* ctx) {
  if (first < last) {
    int left = first, right = last, middle = ctx->numbers[(left + right) / 2];
    do
    {
      while (ctx->numbers[left] < middle) left++;
      while (ctx->numbers[right] > middle) right--;
      if (left <= right) {
        int tmp = ctx->numbers[left];
        ctx->numbers[left] = ctx->numbers[right];
        ctx->numbers[right] = tmp;
        left++;
        right--;
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        if (current_time.tv_nsec - ctx->last_froze.tv_nsec >= ctx->work_time_nanosec) {
          clock_gettime(CLOCK_MONOTONIC, &ctx->last_froze);
          coro_yield();
        }
      }
    } while (left <= right);
    quick_sort(first, right, ctx);
    quick_sort(left, last, ctx);
  }
}


/**
 * Coroutine body. This code is executed by all the coroutines. Here you
 * implement your solution, sort each individual file.
 */
static void*
coroutine_func_f(void *context)
{
	struct my_context *ctx = context;
    FILE* file = fopen(ctx->file_name, "r");
    int* numbers = calloc(10, sizeof(int));
    int number;
    int cap = 10;
    int size = 0;
    /* Read numbers from file */
    while (fscanf(file, "%d ", &number) > 0) {
      if (cap == size) {
        numbers = realloc(numbers, cap * 2);
        cap *= 2;
      }
      numbers[size] = number;
      size++;
    }
    ctx->numbers = numbers;
    ctx->numbers_size = size;
    quick_sort(0, ctx->numbers_size - 1, ctx);

    sorted_arr* result = (sorted_arr *)malloc(sizeof(struct sorted_arr));
    result->arr = numbers;
    result->arr_size = size;

    /* Here I did not delete free numbers, so it will not be deleted*/
	my_context_delete(ctx);

    return (void*)result;
}

int
main(int argc, char **argv)
{
    int number_coro = -1;
    int quantum_coro_nanosec = 10000000;
    char** file_names = malloc(sizeof(char*) * argc);
    int file_names_size = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--coronums") == 0) {
            number_coro = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--quntum") == 0) {
            quantum_coro_nanosec = atoi(argv[i + 1]) * 1000;
            i++;
        } else {
            file_names[file_names_size] = argv[i];
            file_names_size++;
        }
    }
    /* If number of coroutines is not specified then make if equal to number of files*/
    if (number_coro == -1) {
      number_coro = file_names_size;
    }
	/* Initialize our coroutine global cooperative scheduler. */
	coro_sched_init();
	/* Start several coroutines. */
	for (int i = 0; i < number_coro; ++i) {
		/*
		 * The coroutines can take any 'void *' interpretation of which
		 * depends on what you want. Here as an example I give them
		 * some names.
		 */
		char name[16];
		sprintf(name, "coro_%d", i);
		/*
		 * I have to copy the name. Otherwise, all the coroutines would
		 * have the same name when they finally start.
		 */
		coro_new(coroutine_func_f, my_context_new(name, file_names[i], quantum_coro_nanosec));
	}
	/* Wait for all the coroutines to end. */
	struct coro *c;
    sorted_arr** sorted_arrays = malloc(sizeof(int*) * file_names_size);
    int sorted_arrays_size = 0;
	while ((c = coro_sched_wait()) != NULL) {
		/*
		 * Each 'wait' returns a finished coroutine with which you can
		 * do anything you want. Like check its exit status, for
		 * example. Don't forget to free the coroutine afterwards.
		 */

        sorted_arr* sorted_numbers = (sorted_arr*)coro_result(c);
        sorted_arrays[sorted_arrays_size] = sorted_numbers;
        sorted_arrays_size++;
        printf("Finished\n");
		coro_delete(c);
//        printArr(sorted_numbers->arr, sorted_numbers->arr_size, '-', "1");

	}
	/* All coroutines have finished. */

	/* IMPLEMENT MERGING OF THE SORTED ARRAYS HERE. */
    FILE *output = fopen("output.txt", "w");
    int* current_pos = calloc(sorted_arrays_size, sizeof(int));
    for (int i = 0; i < sorted_arrays_size; i++) {
        current_pos[i] = 0;
    }
    int min_number_index;
    int min_number;
    while(true) {
        min_number_index = -1;
        min_number = INT32_MAX;
        for (int i = 0; i < file_names_size; i++) {
            if (current_pos[i] == sorted_arrays[i]->arr_size) {
              continue;
            }
            if (sorted_arrays[i]->arr[current_pos[i]] < min_number) {
              min_number_index = i;
              min_number = sorted_arrays[i]->arr[current_pos[i]];
            }
        }
        if (min_number_index == -1) {
          break;
        }
        fprintf(output, "%d ", min_number);
        current_pos[min_number_index]++;
    }
    free(current_pos);
    free(sorted_arrays);
    free(file_names);
	return 0;
}
