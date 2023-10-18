#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static int
execute_command_line(const struct command_line *line, int* exit_called)
{
	assert(line != NULL);
    FILE* file_out_descriptor = NULL;
	if (line->out_type == OUTPUT_TYPE_STDOUT) {
	} else if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
        file_out_descriptor = fopen(line->out_file, "w");
	} else if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
        file_out_descriptor = fopen(line->out_file, "a+");
	} else {
		assert(false);
	}
    int is_first = 1;
    int global_exit_code = 0;
	const struct expr *e = line->head;
    int next_expr_pipe = 0;
    int next_pipe_descriptors[2];
    int pre_expr_pipe = 0;
    int pre_pipe_descriptors[2];
	while (e != NULL) {
        if (is_first == 1 && strcmp(e->cmd.exe, "exit") == 0 && e->next == NULL) {
            if (e->cmd.arg_count >= 1) {
                int exit_code;
                sscanf(e->cmd.args[0], "%d", &exit_code);
                *exit_called = 1;
                return exit_code;
            } else {
                *exit_called = 1;
                return 0;
            }
        }

		if (e->type == EXPR_TYPE_COMMAND) {
            if (e->next != NULL && e->next->type == EXPR_TYPE_PIPE) {
                next_expr_pipe = 1;
                if (pipe(next_pipe_descriptors) == -1) {
                    printf("Error while created pipe\n");
                    return 1;
                }
            }
            if (strcmp(e->cmd.exe, "cd") == 0) {
                if (e->cmd.arg_count == 0) {
                    printf("Have to provide directory.\n");
                } else {
                    chdir(e->cmd.args[0]);
                }
            } else {
                int pid = fork();
                if (pid == -1) {
                    printf("Error while create fork\n");
                    return 1;
                }
                if (pid == 0) {
                    // Child process
                    if (file_out_descriptor != NULL && e->next == NULL) {
                        dup2(fileno(file_out_descriptor), STDOUT_FILENO);
                        fclose(file_out_descriptor);
                    }
                    if (pre_expr_pipe == 1) {
                        dup2(pre_pipe_descriptors[0], STDIN_FILENO);
                        close(pre_pipe_descriptors[0]);
                        close(pre_pipe_descriptors[1]);
                    }
                    if (next_expr_pipe == 1) {
                        dup2(next_pipe_descriptors[1], STDOUT_FILENO);
                        close(next_pipe_descriptors[0]);
                        close(next_pipe_descriptors[1]);
                    }
                    char** exec_args = (char**)malloc(sizeof(char*) * (e->cmd.arg_count + 2));
                    exec_args[0] = e->cmd.exe;
                    for (uint32_t i = 0; i < e->cmd.arg_count; ++i) {
                        exec_args[i + 1] = e->cmd.args[i];
                    }
                    exec_args[e->cmd.arg_count + 1] = NULL;

                    if (strcmp(e->cmd.exe, "exit") == 0) {
                        if (e->cmd.arg_count >= 1) {
                            int exit_code;
                            sscanf(e->cmd.args[0], "%d", &exit_code);
                            free(exec_args);
                            *exit_called = 1;
                            return exit_code;
                        } else {
                            free(exec_args);
                            *exit_called = 1;
                            return 0;
                        }
                    }
                    int exec_res = execvp(e->cmd.exe, exec_args);
                    if (exec_res < 0) {
                        printf("Error with code %d while execv %s in child fork %d\n", exec_res, e->cmd.exe, getpid());
                    }
                    free(exec_args);
                    return 1;
                } else {
                    // Parent process
                    if (pre_expr_pipe == 1) {
                        close(pre_pipe_descriptors[0]);
                        close(pre_pipe_descriptors[1]);
                    }
                    if (next_expr_pipe == 1) {
                        pre_expr_pipe = 1;
                        next_expr_pipe = 0;
                        pre_pipe_descriptors[0] = next_pipe_descriptors[0];
                        pre_pipe_descriptors[1] = next_pipe_descriptors[1];
                    }

                    if (strcmp(e->cmd.exe, "yes") != 0 && strcmp(e->cmd.exe, "head") != 0) {
                        waitpid(pid, &global_exit_code, 0);
                    }

                    if (e->next == NULL) {
                        fclose(file_out_descriptor);
                    }
                }
            }
		} else if (e->type == EXPR_TYPE_PIPE) {
            // We have already created pipe, so just skip
		} else if (e->type == EXPR_TYPE_AND) {
			printf("\tAND\n");
		} else if (e->type == EXPR_TYPE_OR) {
			printf("\tOR\n");
		} else {
			assert(false);
		}
		e = e->next;
        is_first = 0;
	}
    return global_exit_code / 256;
}

int
main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
    int global_exit_code = 0;
    int exit_called = 0;
	struct parser *p = parser_new();
	while (exit_called == 0 && (rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
		parser_feed(p, buf, rc);
		struct command_line *line = NULL;
		while (true) {
			enum parser_error err = parser_pop_next(p, &line);
			if (err == PARSER_ERR_NONE && line == NULL)
				break;
			if (err != PARSER_ERR_NONE) {
				printf("Error: %d\n", (int)err);
				continue;
			}
			global_exit_code = execute_command_line(line, &exit_called);
            command_line_delete(line);
            if (exit_called != 0) {
                break;
            }
		}
	}
    parser_delete(p);
	return global_exit_code;
}
