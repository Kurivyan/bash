#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <bits/waitflags.h>
#include <errno.h>

struct conv
{
	int commands_count;
	char *start_flag;
	char ***data;
	struct conv *next;
	int bg_flag;
};

struct node
{
	int conv_count;
	int block_and_flag;
	int block_or_flag;
	struct conv *head;
	struct node *next;
};

struct group_pr
{
	pid_t pid;
	int status; // 1 - продолжен || 2 - остановлен || 3 - завершен
	struct group_pr *next;
};

struct group
{
	pid_t group_leader;
	int size;
	int status; // 1 - выполняется || 2 - приостановлена || 3 - завершена || 4 - завершен, отмечен || 5 - убит || 6 - fg
	struct group_pr *gr_pr;
	struct group *next;
};

// < -- -- -- -- -- -- -->
struct node *create_node(char *);
struct conv *create_conv();
struct group *creat_group();
struct group_pr *creat_group_pr(pid_t);
void insert_command(struct node **, char *);
void insert_group(struct group **, struct group *);
void insert_group_pr(struct group_pr **, pid_t);
char *read_func();
char *destructorize(char *);
void execute(struct node *, struct group **);
void execute_conv(struct conv *, struct group **, struct node *);
void file_redirecting(char **);
void return_signals();
void jobs_control(struct group *);
void execute_if_inner(struct conv *, struct group *, int *);
void data_destroyer();
void print_jobs(struct group *);
void return_foreground(pid_t, struct group *);
void analyze_jobs(struct group *);
// < -- -- -- -- -- -- -->

struct group_pr *creat_group_pr(pid_t pid)
{
	struct group_pr *ptr = malloc(sizeof(struct group_pr));
	ptr->pid = pid;
	ptr->status = 0;
	ptr->next = NULL;
}

void insert_group_pr(struct group_pr **head, pid_t target_pid)
{
	if (*head == NULL)
	{
		*head = creat_group_pr(target_pid);
		return;
	}

	struct group_pr *cur = *head;

	while (cur->next != NULL)
	{
		cur = cur->next;
	}
	cur->next = creat_group_pr(target_pid);
}

struct group *creat_group()
{
	struct group *ptr = malloc(sizeof(struct group));
	ptr->group_leader = 0;
	ptr->size = 0;
	ptr->status = 0;
	ptr->gr_pr = NULL;
	ptr->next = NULL;
	return ptr;
}

void insert_group(struct group **head, struct group *insertance)
{
	if (*head == NULL)
	{
		*head = insertance;
		return;
	}
	struct group *cur = *head;
	while (cur->next != NULL)
	{
		cur = cur->next;
	}
	cur->next = insertance;
}

struct conv *create_conv()
{
	struct conv *ptr = calloc(1, sizeof(struct conv));
	char ***temp;
	temp = calloc(5, sizeof(char **));
	for (int i = 0; i < 5; i++)
	{
		temp[i] = calloc(5, sizeof(char *));
	}
	for (int i = 0; i < 5; i++)
	{
		for (int j = 0; j < 7; j++)
		{
			temp[i][j] = calloc(15, sizeof(char));
		}
	}
	ptr->commands_count = 1;
	ptr->start_flag = NULL;
	ptr->data = temp;
	ptr->bg_flag = 0;
	ptr->next = NULL;
	return ptr;
}

struct node *create_node(char *command)
{
	struct node *ptr = calloc(1, sizeof(struct node));
	ptr->conv_count = 1;
	ptr->block_and_flag = 0;
	ptr->block_or_flag = 0;
	ptr->head = create_conv();
	ptr->next = NULL;
	return ptr;
}

char *destructorize(char *command)
{
	if (*command == '\0')
	{
		free(command);
		return NULL;
	}

	char *part = calloc(10, sizeof(char));

	int i = 0;
	int j = 0;
	char c = command[i];
	int quotes = 0;

	while (c != '\0')
	{
		if (c == ' ' && quotes == 0)
		{
			i++;
			break;
		}
		if (c == '\"' && quotes == 0)
		{
			quotes = 1;
		}
		else if (c == '\"' && quotes == 1)
		{
			quotes = 0;
		}
		else
		{
			part[j] = c;
			j++;
		}
		i++;
		c = command[i];
	}

	part[j] = '\0';

	int n_skip = strlen(command) - i;
	memmove(command, command + i, n_skip + 1);

	return part;
}

void insert_command(struct node **head, char *command)
{
	struct node *node = create_node(command);
	struct conv *curent_conv = node->head;
	int i_conv_command = 0;
	int i_conv_command_word = 0;
	char *part = destructorize(command);
	while (1)
	{
		if (part == NULL)
		{
			curent_conv->data[i_conv_command][i_conv_command_word] = NULL;
			break;
		}
		if (strcmp(part, "||") == 0 || strcmp(part, "&&") == 0 || strcmp(part, ";") == 0 || strcmp(part, "&") == 0)
		{
			if (strcmp(part, "&") == 0)
			{
				curent_conv->bg_flag = 1;
				char *test = destructorize(command);
				if (test == NULL)
				{
					curent_conv->data[i_conv_command][i_conv_command_word] = NULL;
					break;
				}
				else
				{
					strcpy(part, test);
					continue;
				}
			}
			node->conv_count += 1;
			curent_conv->next = create_conv();
			curent_conv->data[i_conv_command][i_conv_command_word] = NULL;
			curent_conv = curent_conv->next;
			i_conv_command = 0;
			i_conv_command_word = 0;
			curent_conv->start_flag = calloc(4, sizeof(char));
			strcpy(curent_conv->start_flag, part);
			part = destructorize(command);
			continue;
		}

		if (strcmp(part, "|") == 0)
		{
			curent_conv->commands_count += 1;
			curent_conv->data[i_conv_command][i_conv_command_word] = NULL;
			i_conv_command_word = 0;
			i_conv_command++;
			part = destructorize(command);
			continue;
		}
		strcpy(curent_conv->data[i_conv_command][i_conv_command_word], part);
		free(part);
		i_conv_command_word++;
		part = destructorize(command);
	}
	if (*head == NULL)
	{
		*head = node;
	}
	else
	{
		struct node *cur = *head;
		while (cur->next != NULL)
		{
			cur = cur->next;
		}
		cur->next = node;
		return;
	}
}

char *read_func()
{
	int size = 20;
	char *ptr = calloc(size, sizeof(char));
	char c;
	c = getchar();

	int i = 0;
	int flag = 0;
	int quotes = 0;

	while (c != '\n')
	{
		if (i == size - 1)
		{
			size += 10;
			ptr = realloc(ptr, size * sizeof(char));
		}

		if (c != ' ' && flag == 1)
			flag = 0;

		if (c == ' ' && flag == 1)
		{
			c = getchar();
			continue;
		}

		if (c == ' ' && flag == 0)
		{
			if (quotes != 1)
			{
				flag = 1;
			}
			ptr[i] = c;
			i++;
			c = getchar();
			continue;
		}

		if (c == ' ' && i == 0)
		{
			c = getchar();
			continue;
		}

		if (c == '\"' && quotes == 0)
		{
			quotes = 1;
			ptr[i] = c;
			i++;
			c = getchar();
			continue;
		}

		if (c == '\"' && quotes == 1)
		{
			quotes = 0;
			ptr[i] = c;
			i++;
			c = getchar();
			continue;
		}

		ptr[i] = c;
		i++;
		c = getchar();
	}
	ptr[i] = '\0';

	if (*ptr == '\0')
	{
		free(ptr);
		return NULL;
	}
	return ptr;
}

void file_redirecting(char **data)
{
	char *word;
	int fd;
	int pipe[2];
	int i = 0;
	while (1)
	{
		word = data[i];
		if (word == NULL)
			break;
		if (strcmp(word, ">") == 0)
		{
			fd = open(data[i + 1], O_CREAT | O_WRONLY, 0666);
			dup2(STDOUT_FILENO, pipe[1]);
			close(pipe[1]);
			dup2(fd, STDOUT_FILENO);
			close(fd);
			data[i] = NULL;
		}
		if (strcmp(word, ">>") == 0)
		{
			fd = open(data[i + 1], O_CREAT | O_WRONLY | O_APPEND, 0666);
			dup2(STDOUT_FILENO, pipe[1]);
			close(pipe[1]);
			dup2(fd, STDOUT_FILENO);
			close(fd);
			data[i] = NULL;
		}
		if (strcmp(word, "<") == 0)
		{
			fd = open(data[i + 1], O_CREAT | O_RDONLY);
			dup2(STDIN_FILENO, pipe[0]);
			close(pipe[0]);
			dup2(fd, STDIN_FILENO);
			close(fd);
			data[i] = NULL;
		}
		i++;
	}
}

void return_signals()
{
	signal(SIGINT, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
	signal(SIGTTIN, SIG_DFL);
	signal(SIGTTOU, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
}

void execute_conv(struct conv *conv, struct group **group_head, struct node *node)
{

	if (conv->start_flag != NULL) // разграничение конвееров по || && ;
	{
		if (strcmp(conv->start_flag, "||") == 0 && node->block_or_flag == 1)
		{
			return;
		}
		if (strcmp(conv->start_flag, "&&") == 0 && node->block_and_flag == 1)
		{
			return;
		}
	}

	if (conv->commands_count == 1)
	{
		int succes = 0;
		execute_if_inner(conv, *group_head, &succes);
		if (succes)
			return;
	}

	int conv_size = conv->commands_count;

	struct group *conv_group = NULL;
	if (conv->bg_flag == 1)
	{
		conv_group = creat_group();
		conv_group->size = conv_size;
	}

	int infile = STDIN_FILENO;
	int pid[conv_size];
	int outfile = STDOUT_FILENO;
	int fd[2];

	for (int i = 0; i < conv_size; i++)
	{
		if (i + 1 < conv_size)
		{
			pipe(fd);
			outfile = fd[1];
		}
		else
		{
			outfile = STDOUT_FILENO;
		}
		pid[i] = fork();
		if (pid[i] > 0) // группы & структура jobs для bg
		{
			if (i == 0)
			{
				setpgid(pid[i], pid[i]);
				if (conv->bg_flag == 0)
				{
					// printf("tcstpgrp on 455 : %d\n", tcsetpgrp(STDIN_FILENO, getpgid(pid[0])));
					tcsetpgrp(STDIN_FILENO, getpgid(pid[0]));
				}
				else
				{
				}
			}
			else
			{
				setpgid(pid[i], getpgid(pid[0]));
				if (conv->bg_flag == 0)
				{
					// printf("tcstpgrp on 466 : %d\n", tcsetpgrp(STDIN_FILENO, getpgid(pid[0])));
					tcsetpgrp(STDIN_FILENO, getpgid(pid[0]));
				}
			}
			if (conv->bg_flag == 1)
			{
				if (i == 0)
				{
					insert_group_pr(&(conv_group->gr_pr), pid[i]);
					conv_group->group_leader = conv_group->gr_pr->pid;
					if (conv_size == 1)
					{
						insert_group(group_head, conv_group);
					}
				}
				else
				{
					insert_group_pr(&(conv_group->gr_pr), pid[i]);
					if (i == conv_size - 1)
					{
						insert_group(group_head, conv_group);
					}
				}
			}
		}
		if (pid[i] == 0)
		{
			if (i == 0)
			{
				setpgid(getpid(), getpid());
				if (conv->bg_flag == 0)
				{
					// printf("tcstpgrp on 498 : %d\n", tcsetpgrp(STDIN_FILENO, getpgrp()));
					tcsetpgrp(STDIN_FILENO, getpgrp());
				}
			}
			else
			{
				setpgid(getpid(), getpgid(pid[0]));
				if (conv->bg_flag == 0)
				{
					// printf("tcstpgrp on 506 : %d\n", tcsetpgrp(STDIN_FILENO, getpgrp()));
					tcsetpgrp(STDIN_FILENO, getpgrp());
				}
			}

			return_signals();

			if (infile != STDIN_FILENO)
			{
				dup2(infile, STDIN_FILENO);
				close(infile);
			}
			if (outfile != STDOUT_FILENO)
			{
				dup2(outfile, STDOUT_FILENO);
				close(outfile);
			}
			file_redirecting(conv->data[i]);
			execvp(conv->data[i][0], conv->data[i]);
			exit(1);
		}
		if (infile != STDIN_FILENO)
			close(infile);
		if (outfile != STDOUT_FILENO)
			close(outfile);
		infile = fd[0];
	}
	if (conv->bg_flag == 1)
	{
		return;
	}
	int status;
	for (int i = 0; i < conv->commands_count; i++)
	{
		waitpid(-(pid[0]), &status, WUNTRACED);
		if (!WIFEXITED(status))
		{
			if (WIFSTOPPED(status))
			{
				conv_group = creat_group();
				conv_group->size = conv_size;
				for (int i = 0; i < conv_size; i++)
				{
					insert_group_pr(&(conv_group->gr_pr), pid[i]);
				}
				conv_group->group_leader = conv_group->gr_pr->pid;
				insert_group(group_head, conv_group);
				// printf("tcstpgrp on 551 : %d\n", tcsetpgrp(STDIN_FILENO, getpgrp()));
				node->block_and_flag = 0;
				node->block_or_flag = 0;
				tcsetpgrp(STDIN_FILENO, getpgrp());
				break;
			}
		}
		if (WEXITSTATUS(status) == 0)
		{
			node->block_and_flag = 0;
			node->block_or_flag = 1;
		}
		if (WEXITSTATUS(status) == 1)
		{
			node->block_and_flag = 1;
			node->block_or_flag = 0;
		}
	}
	// printf("tcstpgrp on 566 : %d\n", tcsetpgrp(STDIN_FILENO, getpgrp()));
	tcsetpgrp(STDIN_FILENO, getpgrp());
}

void execute_if_inner(struct conv *conv, struct group *group_head, int *succes)
{
	if (strcmp(conv->data[0][0], "cd") == 0)
	{
		int cd = chdir(conv->data[0][1]);
		if (cd == -1)
		{
			printf("cd : \"Set apropriate path.\"");
		}
		*succes = 1;
	}
	if (strcmp(conv->data[0][0], "quit") == 0)
	{
		data_destroyer();
		kill(getpid(), SIGKILL);
		*succes = 1;
	}
	if (strcmp(conv->data[0][0], "jobs") == 0)
	{
		print_jobs(group_head);
		*succes = 1;
	}
	if (strcmp(conv->data[0][0], "kill") == 0)
	{
		if (conv->data[0][1] == NULL)
		{
			printf("Enter command in kill <sig> <pid>/[-<pgid>]\n");
		}

		if (conv->data[0][2] == NULL)
		{
			printf("Enter command in kill <sig> <pid>/[-<pgid>]\n");
		}

		if (conv->data[0][2][0] == '-')
		{
			kill((atoi(conv->data[0][2])), atoi(conv->data[0][1]));
			if (errno == EINVAL)
			{
				printf("An invalid signal was specified.\n");
			}
			if (errno == EPERM)
			{
				printf("The calling process does not have permission to send the signal to any of the target processes.\n");
			}
			if (errno == ESRCH)
			{
				printf("The target process or process group does not exist.  Note that an existing process might be a zombie, a process that has terminated execution, but  has not yet been wait(2)ed for.\n");
			}
			errno = 0;
		}

		kill((atoi(conv->data[0][2])), atoi(conv->data[0][1]));
		if (errno == EINVAL)
		{
			printf("An invalid signal was specified.\n");
		}
		if (errno == EPERM)
		{
			printf("The calling process does not have permission to send the signal to any of the target processes.\n");
		}
		if (errno == ESRCH)
		{
			printf("The target process or process group does not exist.  Note that an existing process might be a zombie, a process that has terminated execution, but  has not yet been wait(2)ed for.\n");
		}
		errno = 0;
		*succes = 1;
	}
	if (strcmp(conv->data[0][0], "mfg") == 0)
	{
		if (conv->data[0][1] == NULL)
		{
			printf("Use mfg <gpid>\n");
		}
		pid_t target_pid = atoi(conv->data[0][1]);
		return_foreground(target_pid, group_head);
		*succes = 1;
	}
}

void return_foreground(pid_t target_pid, struct group *group_head)
{
	while (group_head->group_leader != target_pid)
	{
		group_head = group_head->next;
	}
	int f_counter = 0;
	while (group_head->gr_pr != NULL)
	{
		if (group_head->gr_pr->status == 3)
			f_counter++;
		group_head->gr_pr = group_head->gr_pr->next;
	}

	tcsetpgrp(STDIN_FILENO, group_head->group_leader);
	for (int i = 0; i < (group_head->size - f_counter); i++)
	{
		int status;
		waitpid(-(group_head->group_leader), &status, WUNTRACED);
		if (!WIFEXITED(status))
		{
			if (WIFSTOPPED(status))
			{
				tcsetpgrp(STDIN_FILENO, getpgrp());
				group_head->status = 2;
				// printf("tcstpgrp on 551 : %d\n", tcsetpgrp(STDIN_FILENO, getpgrp()));
				break;
			}
		}
	}
}

void jobs_control(struct group *group_head)
{
	if (group_head == NULL)
	{
		return;
	}
	while (group_head != NULL)
	{
		if (group_head->status == 5)
		{
			group_head = group_head->next;
			continue;
		}
		int wait_pid = 0;
		int status = 0;
		for (int i = 0; i < group_head->size; i++)
		{
			wait_pid = 0;
			wait_pid = waitpid(-(group_head->group_leader), &status, WUNTRACED | WNOHANG | WCONTINUED);
			if (wait_pid == 0)
			{
			}
			printf("wait_pid = %d \n", wait_pid);
			if (wait_pid == -1)
			{
				group_head->status = 5;
				break;
			}
			if (wait_pid > 1)
			{
				struct group_pr *ptr = group_head->gr_pr;
				while (ptr->pid != wait_pid)
				{
					ptr = ptr->next;
				}
				if (WIFEXITED(status))
				{
					ptr->status = 3;
				}
				else if (!WIFEXITED(status))
				{
					if (WIFSTOPPED(status))
					{
						ptr->status = 2;
					}
					if (WIFCONTINUED(status))
					{
						ptr->status = 1;
					}
				}
			}
		}
		group_head = group_head->next;
	}
}

void analyze_jobs(struct group *group_head)
{
	if (group_head == NULL)
	{
		return;
	}
	struct group *ptr = group_head;
	struct group_pr *ptr_gr = ptr->gr_pr;
	int stop_c = 0;
	int exit_c = 0;
	while (ptr != NULL)
	{
		ptr_gr = ptr->gr_pr;
		stop_c = 0;
		exit_c = 0;
		while (ptr_gr != NULL)
		{
			if (ptr_gr->status == 0 || ptr_gr->status == 1)
			{
				ptr->status = 1;
				break;
			}
			if (ptr_gr->status == 2)
			{
				stop_c++;
			}
			if (ptr_gr->status == 3)
			{
				exit_c++;
			}
			ptr_gr = ptr_gr->next;
		}
		if (stop_c == ptr->size)
		{
			ptr->status = 2;
		}
		else if (stop_c == 0 && stop_c + exit_c == ptr->size)
		{
			ptr->status = 3;
		}
		else if ((stop_c == 0 || exit_c == 0) && stop_c + exit_c != ptr->size)
		{
			ptr->status = 1;
		}
		ptr = ptr->next;
	}
}

void print_jobs(struct group *group_head)
{
	while (group_head != NULL)
	{
		if (group_head->status == 1)
		{
			printf("[%d] Working\n", group_head->group_leader);
		}
		if (group_head->status == 2)
		{
			printf("[%d] Stopped status : %d\n", group_head->group_leader, group_head->status);
		}
		if (group_head->status == 3)
		{
			printf("[%d] Finished\n", group_head->group_leader);
			group_head->status = 4;
		}
		if (group_head->status == 5)
		{
			printf("[%d] Killed\n", group_head->group_leader);
		}
		group_head = group_head->next;
	}
}

void execute(struct node *head, struct group **group)
{
	while (head->next != NULL)
	{
		head = head->next;
	}

	for (int i = 0; i < head->conv_count; i++)
	{
		struct conv *cur_conv = head->head;
		int j = i;
		while (j)
		{
			cur_conv = cur_conv->next;
			j--;
		}
		execute_conv(cur_conv, group, head);
	}
}

void data_destroyer() {}

int main()
{
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	setpgid(getpid(), getpid());
	// printf("tcstpgrp on 654 : %d\n", tcsetpgrp(STDIN_FILENO, getpgrp()));
	tcsetpgrp(STDIN_FILENO, getpgrp());

	struct node *head;
	head = NULL;
	struct group *group_head;
	group_head = NULL;
	char *ptr;
	while (1)
	{
		printf(">\n");
		ptr = read_func();
		if (ptr == NULL)
			continue;
		insert_command(&head, ptr);
		execute(head, &group_head);
		jobs_control(group_head);
		analyze_jobs(group_head);
	}
	return 0;
}
// verified