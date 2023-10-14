#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static void
execute_command_line(const struct command_line *line)
{
	assert(line != NULL);
	if (line->out_type == OUTPUT_TYPE_STDOUT) {

	} else if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
		printf("new file - \"%s\"\n", line->out_file);
	} else if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
		printf("append file - \"%s\"\n", line->out_file);
	} else {
		assert(false);
	}

	const struct expr *e = line->head;
    int next_expr_pipe = 0;
    int next_pipe_descriptors[2];
    int pre_expr_pipe = 0;
    int pre_pipe_descriptors[2];
	while (e != NULL) {
		if (e->type == EXPR_TYPE_COMMAND) {
            if (e->next != NULL && e->next->type == EXPR_TYPE_PIPE) {
                next_expr_pipe = 1;
                if (pipe(next_pipe_descriptors) == -1) {
                    printf("Error while created pipe\n");
                    return;
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
                    return;
                }
                if (pid == 0) {
                    // Child process
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
                        exec_args[i + 1] =e->cmd.args[i];
                    }
                    exec_args[e->cmd.arg_count + 1] = NULL;

                    int exec_res = execvp(e->cmd.exe, exec_args);
                    if (exec_res < 0) {
                        printf("Error with code %d while execv in child fork %d\n", exec_res, getpid());
                    }
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
                    waitpid(pid, NULL, 0);
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
	}
}

int
main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
	struct parser *p = parser_new();
	while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
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
			execute_command_line(line);
			command_line_delete(line);
		}
	}
	parser_delete(p);
	return 0;
}
