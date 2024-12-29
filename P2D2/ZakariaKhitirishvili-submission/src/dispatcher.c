#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dispatcher.h"
#include "shell_builtins.h"
#include "parser.h"
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
/**
 * dispatch_external_command() - run a pipeline of commands
 *
 * @pipeline:   A "struct command" pointer representing one or more
 *              commands chained together in a pipeline.  See the
 *              documentation in parser.h for the layout of this data
 *              structure.  It is also recommended that you use the
 *              "parseview" demo program included in this project to
 *              observe the layout of this structure for a variety of
 *              inputs.
 *
 * Note: this function should not return until all commands in the
 * pipeline have completed their execution.
 *
 * Return: The return status of the last command executed in the
 * pipeline.
 */

//pid_t fork(void);

static int dispatch_external_command(struct command *pipeline)
{
	struct command *current = pipeline;
	/*
	 * Note: this is where you'll start implementing the project.
	 *
	 * It's the only function with a "TODO".  However, if you try
	 * and squeeze your entire external command logic into a
	 * single routine with no helper functions, you'll quickly
	 * find your code becomes sloppy and unmaintainable.
	 *
	 * It's up to *you* to structure your software cleanly.  Write
	 * plenty of helper functions, and even start making yourself
	 * new files if you need.
	 *
	 * For D1: you only need to support running a single command
	 * (not a chain of commands in a pipeline), with no input or
	 * output files (output to stdout only).  In other words, you
	 * may live with the assumption that the "input_file" field in
	 * the pipeline struct you are given is NULL, and that
	 * "output_type" will always be COMMAND_OUTPUT_STDOUT.
	 *
	 * For D2: you'll extend this function to support input and
	 * output files, as well as pipeline functionality.
	 *
	 * Good luck!
	 */

	// check files

	int fd0; // read only input filename
	int fd1; // write only output filename
	int prev_pipe[2];
	int current_pipe[2];
	bool pipeCheck = false;

	while (true) {
		// input types
		if (current->input_filename != NULL) {
			fd0 = open(current->input_filename, O_RDONLY);
			if (fd0 == -1) {
				perror("input file couldn't open");
				//exit("input file couldn't open"); do I need this?????
			}
		}

		if (pipeCheck == true) {
			fd0 = prev_pipe[0];
		}

		//  Output types
		if (current->output_type == COMMAND_OUTPUT_FILE_TRUNCATE) {
			fd1 = open(current->output_filename,
				   O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if (fd1 == -1) {
				perror("output file truncate couldn't open");
			}
		}

		if (current->output_type == COMMAND_OUTPUT_FILE_APPEND) {
			fd1 = open(current->output_filename,
				   O_WRONLY | O_CREAT | O_APPEND, 0666);
			if (fd1 == -1) {
				perror("output file append couldn't open");
			}
		}

		if (current->output_type ==
		    COMMAND_OUTPUT_STDOUT) { // ???????????? STDOUT
		}

		if (current->output_type == COMMAND_OUTPUT_PIPE) {
			if (fd1 == -1) {
				perror("error");
			}
			pipe(current_pipe);
			fd1 = current_pipe[1];
		}

		// below is P1D1 code
		int wstatus;

		int a = fork();
		if (a == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}

		if (a == 0) {
			if (current->output_type != COMMAND_OUTPUT_STDOUT) {
				dup2(fd1, STDOUT_FILENO);
			}
			if (pipeCheck || current->input_filename != NULL) {
				dup2(fd0, STDIN_FILENO);
			}

			if (pipeCheck) {
				int closepipe = close(prev_pipe[0]);
				if(closepipe < 0 ){
				perror("can't close");
			}
			}
			if (current->output_type == COMMAND_OUTPUT_PIPE) {
				int closepipe = close(current_pipe[0]);
				if(closepipe < 0 ){
				perror("can't close");
			}
			}

			execvp(current->argv[0], current->argv);
			perror("error: cannot find command");
			exit(-1);
		} else {
				if (current->output_type == COMMAND_OUTPUT_PIPE) {
				int clsoepipe = close(current_pipe[1]);
				if(clsoepipe < 0 ){
				perror("can't close");
			}
			}
			
			waitpid(a, &wstatus, 0);



			if (WIFEXITED(wstatus)) {
				if (WEXITSTATUS(wstatus) != 0) {
					// perror("error: cannot find command");
				}
				//printf("exited, status=%d\n", WEXITSTATUS(wstatus));
			}
		}
/*
		if (pipeCheck) {
		//	close(prev_pipe[1]);

		}
*/
		if (current->output_type != COMMAND_OUTPUT_PIPE) {
	//		int closepipe = close(prev_pipe[1]);
	//		if(closepipe < 0 ){
	//			perror("can't close");
	//		}
			return WEXITSTATUS(wstatus);
		}

		// if not null
		prev_pipe[0] = current_pipe[0];
		prev_pipe[1] = current_pipe[1];

		pipeCheck = true;
		
		current = current->pipe_to;
	}

} // while loop ends









/**
 * dispatch_parsed_command() - run a command after it has been parsed
 *
 * @cmd:                The parsed command.
 * @last_rv:            The return code of the previously executed
 *                      command.
 * @shell_should_exit:  Output parameter which is set to true when the
 *                      shell is intended to exit.
 *
 * Return: the return status of the command.
 */
static int dispatch_parsed_command(struct command *cmd, int last_rv,
				   bool *shell_should_exit)
{
	/* First, try to see if it's a builtin. */
	for (size_t i = 0; builtin_commands[i].name; i++) {
		if (!strcmp(builtin_commands[i].name, cmd->argv[0])) {
			/* We found a match!  Run it. */
			return builtin_commands[i].handler(
				(const char *const *)cmd->argv, last_rv,
				shell_should_exit);
		}
	}

	/* Otherwise, it's an external command. */
	return dispatch_external_command(cmd);
}

int shell_command_dispatcher(const char *input, int last_rv,
			     bool *shell_should_exit)
{
	int rv;
	struct command *parse_result;
	enum parse_error parse_error = parse_input(input, &parse_result);

	if (parse_error) {
		fprintf(stderr, "Input parse error: %s\n",
			parse_error_str[parse_error]);
		return -1;
	}

	/* Empty line */
	if (!parse_result)
		return last_rv;

	rv = dispatch_parsed_command(parse_result, last_rv, shell_should_exit);
	free_parse_result(parse_result);
	return rv;
}
