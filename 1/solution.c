#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libcoro.h"

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc solution.c libcoro.c
 * $> ./a.out
 */

 typedef enum {
    NOT_SORTED,
    IN_PROGRESS,
    SORTED,
} file_status;

struct number_array {
	int* numbers;
	int number_size;
};

struct file_node {
	char* file_name;
	file_status status;
	struct number_array* sorted_array;
	struct file_node* next;
};

struct my_context {
	char *name;
	struct file_node *files;
	int file_names_size;
    long work_time_nanosec;
    struct timespec last_yield;
};

static struct my_context *
my_context_new(const char *name, struct file_node *files, int files_size, long work_time_nanosec)
{
	struct my_context *ctx = malloc(sizeof(*ctx));
	ctx->name = strdup(name);
	ctx->files = files;
	ctx->file_names_size = files_size;
    ctx->work_time_nanosec = work_time_nanosec;
    clock_gettime(CLOCK_MONOTONIC, &ctx->last_yield);
	return ctx;
}

void quick_sort(int first, int last, struct my_context* ctx, int* numbers) {
  if (first < last) {
    int left = first, right = last, middle = numbers[(left + right) / 2];
    do
    {
      while (numbers[left] < middle) left++;
      while (numbers[right] > middle) right--;
      if (left <= right) {
        int tmp = numbers[left];
        numbers[left] = numbers[right];
        numbers[right] = tmp;
        left++;
        right--;
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        if (current_time.tv_nsec - ctx->last_yield.tv_nsec >= ctx->work_time_nanosec) {
          clock_gettime(CLOCK_MONOTONIC, &ctx->last_yield);
          coro_yield();
        }
      }
    } while (left <= right);
    quick_sort(first, right, ctx, numbers);
    quick_sort(left, last, ctx, numbers);
  }
}

struct number_array* read_numbers_from_file(char* file_name) {
	printf("Start read numbers from %s\n", file_name);
    FILE* file = fopen(file_name, "r");
    int* numbers = (int *)calloc(10, sizeof(int));
    int number;
    int cap = 10;
    int size = 0;

    while (fscanf(file, "%d", &number) > 0) {
		if (cap == size) {
			cap *= 2;
			numbers = realloc(numbers, cap * sizeof(int));
      	}
      	numbers[size] = number;
      	size++;
    }
	struct number_array* numb_arr = (struct number_array*)malloc(sizeof(struct number_array));
	numb_arr->number_size = size;
	numb_arr->numbers = numbers;
	printf("End read numbers from %s\n", file_name);
	return numb_arr;
}

static void
my_context_delete(struct my_context *ctx)
{
	free(ctx->name);
	free(ctx);
}

static void*
coroutine_func_f(void *context)
{
	struct my_context *ctx = context;
	printf("%s started\n", ctx->name);
	struct file_node* curr_file = ctx->files;
	while (curr_file != NULL) {
		if (curr_file->status == NOT_SORTED) {
			curr_file->status = IN_PROGRESS;
			struct number_array* number_array = read_numbers_from_file(curr_file->file_name);
			printf("Start sorting file %s\n", curr_file->file_name);
			quick_sort(0, number_array->number_size - 1, ctx, number_array->numbers);
			printf("End sorting file %s\n", curr_file->file_name);
			curr_file->sorted_array = number_array;
			curr_file->status = SORTED;
		}
		curr_file = curr_file->next;
	}

	printf("%s finished\n", ctx->name);
	my_context_delete(ctx);
	return NULL;
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
	quantum_coro_nanosec /= number_coro;

	struct file_node* files = (struct file_node*)malloc(sizeof(struct file_node));
	struct file_node* curr_file = files;
	curr_file->file_name = file_names[0];
	curr_file->status = NOT_SORTED;
	curr_file->next = NULL;
	curr_file->sorted_array = NULL;
	for (int i = 1; i < file_names_size; i++) {
		struct file_node* new_file = (struct file_node*)malloc(sizeof(struct file_node));
		new_file->file_name = file_names[i];
		new_file->status = NOT_SORTED;
		new_file->next = NULL;
		new_file->sorted_array = NULL;
		curr_file->next = new_file;
		curr_file = new_file;
	}

	coro_sched_init();
	for (int i = 0; i < number_coro; ++i) {
		char name[16];
		sprintf(name, "coro_%d", i);
		coro_new(coroutine_func_f, my_context_new(name, files, file_names_size, quantum_coro_nanosec));
	}
	printf("Corotines created\n");

	struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		coro_delete(c);
	}
	/* All coroutines have finished. */

	/* Merging numbers from files */
	FILE *output = fopen("output.txt", "w");
    int* current_pos = calloc(file_names_size, sizeof(int));
    for (int i = 0; i < file_names_size; i++) {
        current_pos[i] = 0;
    }
    int min_number_index;
    int min_number;
	int index = 0;
    while(true) {
        min_number_index = -1;
        min_number = INT32_MAX;
		curr_file = files;
		index = 0;
        while (curr_file != NULL) {
            if (current_pos[index] == curr_file->sorted_array->number_size) {
				curr_file = curr_file->next;
				continue;
            }
            if (curr_file->sorted_array->numbers[current_pos[index]] < min_number) {
            	min_number_index = index;
            	min_number = curr_file->sorted_array->numbers[current_pos[index]];
            }
			curr_file = curr_file->next;
        }
        if (min_number_index == -1) {
        	break;
        }
        fprintf(output, "%d ", min_number);
        current_pos[min_number_index]++;
    }
    free(current_pos);
	curr_file = files;
	while (files != NULL) {
		curr_file = curr_file->next;
		free(files->sorted_array->numbers);
		free(files->sorted_array);
		free(files);
		files = curr_file;
	}
    free(file_names);
	return 0;
}