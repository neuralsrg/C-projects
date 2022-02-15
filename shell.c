#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/* Pipeline struct */
typedef struct node {
	/* Number of inp_streams = number of out_streams */
	int n_streams;
	/* Points to the first descriptor of the descriptors array */
	int *inp_streams0;
	int *inp_streams1;
	int *out_streams0;
	int *out_streams1;
	/* Number of the different progs in the current node */
	int n_progs;
	/* Prog + Command line arguments */
	int n_args;
	char **args;
	/* Level's linkers */
	pid_t *linkers;
} N;

/* Command line struct */
typedef struct com {
	struct com *next;
	char *command;
} C;

/* Commands struct */
typedef struct arguments {
	struct arguments *next;
	struct com *pargs;
} A;

N *init_pipe(int *number_nodes, A *programs, const char *line);
void fill_nodes(N *start, int number_nodes, A *programs);
int count_pipes(N *start, int number_nodes);
void handle_pipes(N *start, int number_nodes);
void gen_progs(N *start, int struct_num, int number_nodes, C *progs);
void linkers(N *start, int struct_num, int number_nodes);
void node_processes(N *start, int number_nodes, A *programs, char *line);
void close_d(N *start, int number_nodes, int special_node, int special_stream);
void print_nodes(N *start, int number_nodes);
void writer(N *start, int number_nodes, const char *line);
void kill_linkers(N *start, int struct_num);
A *handle_line(const char *line);
void split_line(C **cmd, const char *command, char delimiter);
void split_progs(A **programs, C *cmd);
char *get_prog(int stream_id, C *progs);
void split_args(C **cmd, const char *command);
void sig_hndlr(int signal);
char *read_line();
char check_daemon(char *line);
char *check_file(char *mode, const char *orig_line, char target);
void free_n(N *start, int number_nodes);

int main()
{
	/* SIGUSR1 handler for exit */
	signal(SIGUSR1, sig_hndlr);
	while (1) {
		/* Main process */
		if (!fork()) {
			int number_nodes;
			char *line = read_line();
			if (check_daemon(line)) {
				if (fork()) {
					_exit(0);
				}
			}
			A *programs = handle_line(line);
			N* start = init_pipe(&number_nodes, programs, line);
			fill_nodes(start, number_nodes, programs);
			handle_pipes(start, number_nodes);
			//print_nodes(start, number_nodes);
			node_processes(start, number_nodes, programs, line);
		}
		/* Waiting for the main process */
		wait(NULL);
	}
	return 0;
}

/* Inits the pipeline - array of N structures */
N *init_pipe(int *number_nodes, A* programs, const char *line)
{
	/* Counting number_nodes */
	int i = 0;
	char mode, *filename;
	while (programs) {
		i++;
		programs = programs->next;
	}

	*number_nodes = i;

	/* The 1st node is special */
	N *start = calloc(*number_nodes + 1, sizeof(N));

	start[0].n_streams = 1;
	/* Redirecting stdin to the start[0].out_streams[0] */
	int stdin_fd;
	filename = check_file(NULL, line, '<');

	if (filename) {
		stdin_fd = open(filename, O_RDONLY);
		if (stdin_fd < 0) {
			fprintf(stderr, "BAD FILENAME!\n");
			fflush(stderr);
			free(filename);
			_exit(1);
		}
		free(filename);
	} else {
		stdin_fd = dup(0);
	}
	start[0].out_streams0 = calloc(1, sizeof(int));
	start[0].out_streams0[0] = stdin_fd;
	/* 1st Linker */
	start[0].linkers = calloc(1, sizeof(pid_t));
	
	return start;
}

/* Fills each node with n_progs and n_streams values */
void fill_nodes(N *start, int number_nodes, A *programs)
{
	int i = 0, prog_counter = 0;
	C *cmd;

	while (programs) {
		i++;
		cmd = programs->pargs;
		while (cmd) {
			prog_counter++;
			cmd = cmd->next;
		}
		start[i].n_progs = prog_counter;
		prog_counter = 0;
		programs = programs->next;
	}

	for (i = 1; i <= number_nodes; i++) {
		start[i].n_streams = start[i - 1].n_streams * start[i].n_progs;
		start[i].linkers = calloc(start[i].n_streams, sizeof(pid_t));
	}
}

/* Computes number of pipes needed */
int count_pipes(N *start, int number_nodes)
{
	int node, sum = 0;
	for (node = 1; node <= number_nodes; node++) {
		sum += start[node].n_streams * 2;	
	}
	return sum;	
}

/* Creates pipes and fills each node with them */
void handle_pipes(N *start, int number_nodes)
{
	int number_pipes = count_pipes(start, number_nodes);
	int pipe_array[number_pipes][2];
	int node_id, stream_id, pipe_id = 0;

	for (node_id = 1; node_id <= number_nodes; node_id++) {
		start[node_id].inp_streams0 = calloc(start[node_id].n_streams, sizeof(int));
		start[node_id].inp_streams1 = calloc(start[node_id].n_streams, sizeof(int));
		start[node_id].out_streams0 = calloc(start[node_id].n_streams, sizeof(int));
		start[node_id].out_streams1 = calloc(start[node_id].n_streams, sizeof(int));
		for (stream_id = 0; stream_id < start[node_id].n_streams; stream_id++) {
			pipe(pipe_array[pipe_id]);
			start[node_id].inp_streams0[stream_id] = pipe_array[pipe_id][0];
			start[node_id].inp_streams1[stream_id] = pipe_array[pipe_id][1];
			pipe_id++;
			pipe(pipe_array[pipe_id]);
			start[node_id].out_streams0[stream_id] = pipe_array[pipe_id][0];
			start[node_id].out_streams1[stream_id] = pipe_array[pipe_id][1];
			pipe_id++;
		}
	}
}

/* Creates parent processes for progs */
void node_processes(N *start, int number_nodes, A *programs, char *line)
{
	int node_id, stream_id, sbuf;
	const int BUF_LEN = 100;
	char *buf = calloc(BUF_LEN + 1, sizeof(char));
	A *prog_runner = programs;

	/* 0 node linker handles stdin */
	linkers(start, 0, number_nodes);
	/* For each node */
	for (node_id = 1; node_id <= number_nodes; node_id++) {
		/* Creating node process */
		if (!fork()) {
			gen_progs(start, node_id, number_nodes, prog_runner->pargs);
			/* Closing descriptors */
			close_d(start, number_nodes, -1, -1);
			/* Waiting for prog processes */
			while (wait(NULL) != -1);
			/* Killing linkers, which are not in use anymore */
			kill_linkers(start, node_id - 1);
			_exit(0);
		}
		/* Creating linker processes */
		if (node_id != number_nodes) {
			linkers(start, node_id, number_nodes);
		}
		/* Switching to the next node */
		prog_runner = prog_runner->next;
	}
	/* Creating writer process which will print output */
	writer(start, number_nodes, line);
	/* Freeing line */
	free(line);
	/* Closing descriptors */
	close_d(start, number_nodes, -1, -1);
	/* Waiting for node processes */
	while (wait(NULL) != -1);
	/* Freeing struct */
	free_n(start, number_nodes);
	_exit(0);
}

/* Creates process for each prog */
void gen_progs(N *start, int struct_num, int number_nodes, C *progs)
{
	int stream_id, arg, i;
	char *pr;
	
	/* For each output stream */
	for (stream_id = 0; stream_id < start[struct_num].n_streams; stream_id++) {

		if (!fork()) {
			/* Program to be executed */
			pr = get_prog(stream_id, progs);

			/* Splitting string into arguments */
			C *arg_runner, *args = NULL;
			split_args(&args, pr);
			/* Computing number of arguments */
			int arg_counter = 0;
			arg_runner = args;
			while (arg_runner) {
				arg_counter++;
				arg_runner = arg_runner->next;
			}
			arg_runner = args;
			/* Creating argv[] */
			char **argv;
			argv = calloc(arg_counter + 1, sizeof(char *));
			for (i = 0; i < arg_counter; i++) {
				argv[i] = arg_runner->command;
				arg_runner = arg_runner->next;
			}
			argv[arg_counter] = NULL;

			/* Redirecting I/O streams */
			dup2(start[struct_num].inp_streams0[stream_id], 0);
			dup2(start[struct_num].out_streams1[stream_id], 1);

			/* Closing pipes */
			close_d(start, number_nodes, -1, -1);
			
			/* Executing... */
			execvp(argv[0], argv);
			_exit(1);
		}
	}
}

/* Links start[struct_num] out_streams to the start[struct_num + 1] inp_streams */
void linkers(N *start, int struct_num, int number_nodes)
{
	fflush(stdout);
	int stream_id, next_prog_id, sbuf, index;
	const int BUF_LEN = 100;
	pid_t pid;
	
	/* For each output stream */
	for (stream_id = 0; stream_id < start[struct_num].n_streams; stream_id++) { 

		if (!(pid = fork())) {
			/* Closing unused descriptors */
			close_d(start, number_nodes, struct_num, stream_id);
			/* Redirecting current stream */
			/* Each linker process has it's own buffer */
			//printf("I'm linker and my descriptors are:\n");
			//print_nodes(start, number_nodes);
			char *buf = calloc(BUF_LEN + 1, sizeof(char));
			while ((sbuf = read(start[struct_num].out_streams0[stream_id], buf, BUF_LEN)) > 0) {
				/* For each prog from next level progs */
				for (next_prog_id = 0; next_prog_id < start[struct_num + 1].n_progs; next_prog_id++) {
					index = stream_id * start[struct_num + 1].n_progs + next_prog_id;
					write(start[struct_num + 1].inp_streams1[index], buf, sbuf);
				}
			}

			/* Freeing buffer */
			free(buf);
			/* Closing start[struct_num + 1].inp_streams1 */
			close(start[struct_num].out_streams0[stream_id]);
			for (next_prog_id = 0; next_prog_id < start[0].n_streams; next_prog_id++) {
				index = stream_id * start[struct_num + 1].n_progs + next_prog_id;
				close(start[struct_num + 1].inp_streams1[index]);
			}

			_exit(0);
		}
		start[struct_num].linkers[stream_id] = pid;
	}
}

/* Closes all descriptors for the process */
void close_d(N *start, int number_nodes, int special_node, int special_stream)
{
	int node_id, stream_id, prog_id;

	/* Closing input descriptors */
	if (special_node != 0) {
		close(start[0].out_streams0[0]);
	}
	/* For each node */
	for (node_id = 1; node_id <= number_nodes; node_id++) {
		/* For each stream */
		for (stream_id = 0; stream_id < start[node_id].n_streams; stream_id++) {
			if ((special_node == -1)
					|| (node_id != special_node) && (node_id != special_node + 1)
					|| ((node_id == special_node) && (stream_id != special_stream))
					|| ((node_id == special_node + 1)
						&& ((stream_id < special_stream * start[node_id].n_progs)
							|| (stream_id >= (special_stream + 1) * start[node_id].n_progs)))) {
				close(start[node_id].inp_streams0[stream_id]);
				close(start[node_id].inp_streams1[stream_id]);
				close(start[node_id].out_streams0[stream_id]);
				close(start[node_id].out_streams1[stream_id]);
			} else if (node_id == special_node) {
				close(start[node_id].inp_streams0[stream_id]);
				close(start[node_id].inp_streams1[stream_id]);
				close(start[node_id].out_streams1[stream_id]);
			} else {
				close(start[node_id].inp_streams0[stream_id]);
				close(start[node_id].out_streams0[stream_id]);
				close(start[node_id].out_streams1[stream_id]);
			} 
		}
	}
}

/* TEST FUNCTION: Prints file descriptors */
void print_nodes(N *start, int number_nodes)
{
	int node_id, stream_id;

	for (node_id = 1; node_id <= number_nodes; node_id++) {
		printf("NODE[%d]\n", node_id);
		for (stream_id = 0; stream_id < start[node_id].n_streams; stream_id++) {
			printf("%d %d | %d %d\n", start[node_id].inp_streams0[stream_id], 
					start[node_id].inp_streams1[stream_id],
					start[node_id].out_streams0[stream_id],
					start[node_id].out_streams1[stream_id]);
		}
	}
}

/* Creates a process, which reads the last pipeline output and writes it to the source */
void writer(N *start, int number_nodes, const char *line)
{
	int stream_id, sbuf;
	const int BUF_LEN = 1;
	char mode, *filename;
	int fd;
	filename = check_file(&mode, line, '>');
	if (filename) {
		if (mode) {
			fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, 0666);
		} else {
			fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);
		}
		free(filename);
	} else {
		fd = dup(1);
	}

	if (!fork()) {
		/* Closing descriptors we are not supposed to use */
		close_d(start, number_nodes - 1, -1, -1);
		for (stream_id = 0; stream_id < start[number_nodes].n_streams; stream_id++) {
			close(start[number_nodes].inp_streams0[stream_id]);
			close(start[number_nodes].inp_streams1[stream_id]);
			close(start[number_nodes].out_streams1[stream_id]);
		}
		//printf("I'm writer and my descriptors are:\n");
		//print_nodes(start, number_nodes);
		char *buf = calloc(BUF_LEN + 1, sizeof(char));
		for (stream_id = 0; stream_id < start[number_nodes].n_streams; stream_id++) {
			while ((sbuf = read(start[number_nodes].out_streams0[stream_id], buf, BUF_LEN)) > 0) {
				write(fd, buf, sbuf);
			}
		}
		close(fd);
		free(buf);
		_exit(0);
	}
	close(fd);
}

/* Kills all the linkers from [struct_num] node */
void kill_linkers(N *start, int struct_num)
{
	int stream_id;

	for (stream_id = 0; stream_id < start[struct_num].n_streams; stream_id++) {
		kill(start[struct_num].linkers[stream_id], SIGKILL);
	}
	//printf("%d linkers were killed\n", start[struct_num].n_streams);
}

/* Reads line */
char *read_line()
{
    char *line = NULL;
    size_t n = 0;
	printf("$ ");
	getline(&line, &n, stdin);
	if (!strcmp(line, "exit\n")) {
		free(line);
		kill(getppid(), SIGUSR1);
		_exit(0);
	}
	return line;
}

/* Handles the input line */
A *handle_line(const char *line)
{
	C *cmd = NULL;
	A *programs = NULL;

	split_line(&cmd, line, '|');
	split_progs(&programs, cmd);
	//free(line);
	return programs;
}

/* Splits entire line into pipeline nodes */
void split_line(C **cmd_struct, const char *orig_command, char delimiter)
{
	int i, len = 0, max = strlen(orig_command);
	char *command = calloc(max + 1, sizeof(char));
	strcpy(command, orig_command);
	char changed_flag = 0, skip = 0, *s = command;
	C *cur;

	// <= ???
	for (i = 0; i < max; i++) {
		if ((command[i] == delimiter) || (command[i] == '\n') || (command[i] == '&')
				|| (command[i] == '>') || (command[i] == '<')) {
			/* If < [filename] found */
			if (skip) {
				if (command[i] == delimiter) {
					skip = 0;
					s = command + i + 2;
					continue;
				} else {
					break;
				}
			}
			if (!*cmd_struct) {
				*cmd_struct = malloc(sizeof(C));
				cur = *cmd_struct;
			} else {
				cur->next = malloc(sizeof(C));
				cur = cur->next;
			}
			cur->next = NULL;
			cur->command = calloc(len, sizeof(char));
			if ((command[i] == '&') || (command[i] == '>') || (command[i] == '<')) {
				changed_flag = command[i];
				command[i - 1] = '\n';
				command[i] = '\0';
				strcpy(cur->command, s);
				command[i - 1] = ' ';
				command[i] = changed_flag;
				changed_flag = 0;
				if (command[i] == '<') {
					skip = 1;
					len = -1;
					continue;
				}
				break;
			}
			if (command[i] == delimiter) {
				changed_flag = 1;
				command[i - 1] = '\n';
				command[i] = '\0';
			}
			strcpy(cur->command, s);
			if (changed_flag) {
				changed_flag = 0;
				command[i - 1] = ' ';
				command[i] = delimiter;
			}
			len = -1;
			s = command + i + 2;
		} else 
			len++;
	}
	free(command);
}

/* Splits each node into progs */
void split_progs(A **programs, C *cmd)
{
	C *cur_node = NULL;
	A *cur_a = *programs;
	while (cmd) {
		if (!*programs) {
			*programs = malloc(sizeof(A));
			cur_a = *programs;
		} else {
			cur_a->next = malloc(sizeof(A));
			cur_a = cur_a->next;
		}	
		cur_a->next = NULL;
		split_line(&cur_node, cmd->command, '+');
		cur_a->pargs = cur_node;
		cur_node = NULL;
		cmd = cmd->next;
	}
}

/* Splits arguments */
void split_args(C **cmd, const char *orig_command)
{
	int max, i = 0, len = 0;
	max = strlen(orig_command);
	char *command = calloc(max + 1, sizeof(char));
	strcpy(command, orig_command);
	char cur_char, *s = command;
	C *cur;

	for (i = 0; i <= max; i++) {
		if ((command[i] == ' ') || (command[i] == '\n')) {
			if (!*cmd) {
				*cmd = malloc(sizeof(C));
				cur = *cmd;
			} else {
				cur->next = malloc(sizeof(C));
				cur = cur->next;
			}
			cur->next = NULL;
			cur->command = calloc(len + 1, sizeof(char));
			if (command[i] == ' ') {
				command[i] = '\0';
				strcpy(cur->command, s);
				command[i] = ' ';
			} else {
				command[i] = '\0';
				strcpy(cur->command, s);
				command[i] = '\n';
			}
			len = 0;
			s = command + i + 1;
		}
		else 
			len++;
	}
	free(command);
}

/* Returns prog for the stream_id process */
char *get_prog(int stream_id, C *progs)
{
	C *runner = progs;
	int len = 0;

	while (runner) {
		len++;
		runner = runner->next;
	}
	runner = progs;
	stream_id %= len;
	while (stream_id) {
		runner = runner->next;
		stream_id--;
	}
	return runner->command;
}

/* SIGUSR1 handler */
void sig_hndlr(int signal)
{
	wait(NULL);
	exit(0);
}

/* Checks whether the process should become daemon */
char check_daemon(char *line)
{
	int len = strlen(line);
	if (line[len - 2] == '&') {
		//printf("I'm daemon\n");
		int fd = open("/dev/null", O_RDONLY);
		dup2(fd, 0);
		close(fd);
		fd = open("/dev/null", O_WRONLY);
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);
		setpgid(0, 0);
		return 1;
	}
	return 0;
}

/* Checks whether the output file exists, defines write mode
 * mode = 0 => write
 * mode = 1 => append
 * */
char *check_file(char *mode, const char *orig_line, char target)
{
	int len = 0, i = 0, max = strlen(orig_line);
	char symb, *s = NULL, *filename, *line = calloc(max + 1, sizeof(char));
	strcpy(line, orig_line);
	
	while ((i < max) && (line[i] != target)) {
		i++;
	}
	if (i == max) {
		free(line);
		return NULL;
	}
	if (target == '<') {
		/* Read */
		i += 2;
	} else if (line[i + 1] == target) {
		/* Append */
		i += 3;
		*mode = 1;
	} else {
		/* Write */
		i += 2;
		*mode = 0;
	}
	s = line + i;
	while ((line[i] != ' ') && (line[i] != '\n')) {
		len++;
		i++;
	}
	/* End of filename */
	symb = line[i];
	line[i] = '\0';
	filename = calloc(len + 1, sizeof(char));
	strcpy(filename, s);
	line[i] = symb;
	free(line);

	return filename;
}

/* Freeing structures */

void free_n(N *start, int number_nodes)
{
	
	if (!start) {
		return;
	}
	int i;
	while (number_nodes > 0) {
		free(start[number_nodes].inp_streams0);
		free(start[number_nodes].inp_streams1);
		free(start[number_nodes].out_streams0);
		free(start[number_nodes].out_streams1);
		free(start[number_nodes].linkers);
		number_nodes--;
	}
	free(start[0].out_streams0);
	free(start[0].linkers);
	free(start);
}
