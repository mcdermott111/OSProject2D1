#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dispatcher.h"
#include "shell_builtins.h"
#include "parser.h"
#include <fcntl.h>

#define BUFFER_SIZE PIPE_BUF;

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
static int dispatch_external_command(struct command *pipeline)
{
	int status;
	int curr[2];
	int prev[2];
	// put in while loop
	struct command *temp = pipeline;
	int x = 0;
	while (true)
	{
		//printf("%d \n", x);
		
		int outputFileDesc = STDOUT_FILENO;
		if (temp->output_type == COMMAND_OUTPUT_PIPE)
		{
			outputFileDesc = pipe(curr);
			
			if (outputFileDesc < 0) {
				exit(EXIT_FAILURE);
			}
			outputFileDesc = curr[1];
		}
		if (temp->output_type == COMMAND_OUTPUT_FILE_APPEND) {
			outputFileDesc = open(temp->input_filename, O_APPEND);
		}
		if (temp->output_type == COMMAND_OUTPUT_FILE_TRUNCATE) {
			outputFileDesc = open(temp->input_filename, O_TRUNC);
		}
		pid_t pid = fork();
		
		// determine which output type it is and select a function to run that
		
		

		if (pid == 0)
		{
			//printf("First  \n");
			int inputFileDesc = STDIN_FILENO;
			if (temp->output_filename == NULL && x > 0) {
				inputFileDesc = prev[0];
			}else {
				inputFileDesc = open(temp->input_filename, O_RDONLY);
			}
			// if (temp->output_filename != NULL) {
			// 	outputFileDesc = open
			// }
			dup2(outputFileDesc, STDOUT_FILENO);
			dup2(inputFileDesc, STDIN_FILENO);
			if (temp->output_type == COMMAND_OUTPUT_PIPE)
			{
				close(curr[0]);
			}
			if (x > 0) {
				close(prev[0]);
			}
			execvp(temp->argv[0], temp->argv);
		}
		else
		{
			//printf("Second \n");
			
			if (temp->output_type == COMMAND_OUTPUT_PIPE)
			{
				close(curr[1]);
			}
			if (x > 0)
			{
				close(prev[0]);
			}
			if (waitpid(pid, &status, 0) <= 0)
			{
				fprintf(stderr, "Wait Failed");
				exit(0);
			}
			if (temp->output_type != COMMAND_OUTPUT_PIPE) {
				break;
			}
			temp = temp->pipe_to;
		}
		x++;
		
		prev[0] = curr[0];
		prev[1] = curr[1];
	}
	if (WIFEXITED(status) && WEXITSTATUS(status))
	{
		return (WEXITSTATUS(status));
	}
	return 0;
}

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
	for (size_t i = 0; builtin_commands[i].name; i++)
	{
		if (!strcmp(builtin_commands[i].name, cmd->argv[0]))
		{
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

	if (parse_error)
	{
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
