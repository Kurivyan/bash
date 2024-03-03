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
	int x;
	int y;
	int z;
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
	int status; // 1 - продолжен || 2 - остановлен || 3 - завершен || 4 - killed
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
struct node *create_node();
struct conv *create_conv();
struct group *creat_group();
struct group_pr *creat_group_pr(pid_t);
void insert_command(struct node **, char *);
void insert_group(struct group **, struct group *);
void insert_group_pr(struct group_pr **, pid_t);
char *read_func();
char *destructorize(char *);
void execute(struct node *, struct group **, int *);
void execute_conv(struct conv *, struct group **, struct node *, int *);
void file_redirecting(char **);
void return_signals();
void jobs_control(struct group *);
void execute_if_inner(struct conv *, struct group *, int *, int *);
void data_destroyer(struct node **, struct group **);
void destroy_node(struct node *);
void destroy_group(struct group *);
void print_jobs(struct group *);
void return_foreground(pid_t, struct group *);
void analyze_jobs(struct group *);
void print_jobs_expanded(struct group *);
// < -- -- -- -- -- -- -->

struct node *head;
struct group *group_head;

struct group_pr *creat_group_pr(pid_t pid)
{
	struct group_pr *ptr = malloc(sizeof(struct group_pr));
	ptr->pid = pid;
	ptr->status = 0;
	ptr->next = NULL;
	return ptr;
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
	cur = NULL;
	return;
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
	cur = NULL;
	return;
}

struct conv *create_conv()
{
	struct conv *ptr = calloc(1, sizeof(struct conv));
	ptr->x = 7;
	ptr->y = 7;
	ptr->z = 15;
	char ***temp = (char ***)calloc(ptr->x, sizeof(char **));
	for (int i = 0; i < ptr->x; i++)
	{
		temp[i] = (char **)calloc(ptr->y, sizeof(char *));
		for (int j = 0; j < ptr->y; j++)
		{
			temp[i][j] = (char *)calloc(ptr->z, sizeof(char));
		}
	}
	ptr->commands_count = 1;
	ptr->start_flag = NULL;
	ptr->data = temp;
	ptr->bg_flag = 0;
	ptr->next = NULL;
	return ptr;
}

struct node *create_node()
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
			free(curent_conv->data[i_conv_command][i_conv_command_word]);
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
					free(curent_conv->data[i_conv_command][i_conv_command_word]);
					curent_conv->data[i_conv_command][i_conv_command_word] = NULL;
					free(part);
					free(test);
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
			free(curent_conv->data[i_conv_command][i_conv_command_word]);
			curent_conv->data[i_conv_command][i_conv_command_word] = NULL;
			curent_conv = curent_conv->next;
			i_conv_command = 0;
			i_conv_command_word = 0;
			curent_conv->start_flag = calloc(4, sizeof(char));
			strcpy(curent_conv->start_flag, part);
			free(part);
			part = destructorize(command);
			continue;
		}

		if (strcmp(part, "|") == 0)
		{
			curent_conv->commands_count += 1;
			free(curent_conv->data[i_conv_command][i_conv_command_word]);
			curent_conv->data[i_conv_command][i_conv_command_word] = NULL;
			i_conv_command_word = 0;
			i_conv_command++;
			if (i_conv_command == curent_conv->x - 1) // для количества команд
			{
				int new_size = curent_conv->x + 5;
				char ***temp = (char ***)calloc(new_size, sizeof(char **));
				for (int i = 0; i < new_size; i++)
				{
					temp[i] = (char **)calloc(curent_conv->y, sizeof(char *));
					for (int j = 0; j < curent_conv->y; j++)
					{
						temp[i][j] = (char *)calloc(curent_conv->z, sizeof(char));
					}
				}

				for (int i = 0; i < curent_conv->x; i++)
				{
					for (int j = 0; j < curent_conv->y; j++)
					{
						if (curent_conv->data[i][j] != NULL)
							strcpy(temp[i][j], curent_conv->data[i][j]);
					}
				}

				for (int i = 0; i < curent_conv->x; i++)
				{
					for (int j = 0; j < curent_conv->y; j++)
					{
						free(curent_conv->data[i][j]);
					}
					free(curent_conv->data[i]);
					curent_conv->data[i] = NULL;
				}
				free(curent_conv->data);
				curent_conv->data = temp;
				curent_conv->x = new_size;
			}
			free(part);
			part = destructorize(command);
			continue;
		}
		if ((int)strlen(part) > curent_conv->z) // для количества букв
		{
			for (int i = 0; i < curent_conv->x; i++)
			{
				for (int j = 0; j < curent_conv->y; j++)
				{
					if (curent_conv->data[i][j] != NULL)
						curent_conv->data[i][j] = realloc(curent_conv->data[i][j], strlen(part));
				}
			}
			curent_conv->z = (int)strlen(part);
		}
		strcpy(curent_conv->data[i_conv_command][i_conv_command_word], part);
		free(part);
		i_conv_command_word++;
		if (i_conv_command_word == curent_conv->y - 1) // для количества аргументов
		{
			char ***temp;
			int new_size = curent_conv->y + 5;
			temp = (char ***)calloc(curent_conv->x, sizeof(char **));
			for (int i = 0; i < curent_conv->x; i++)
			{
				temp[i] = (char **)calloc(new_size, sizeof(char *));
				for (int j = 0; j < new_size; j++)
				{
					temp[i][j] = (char *)calloc(curent_conv->z, sizeof(char));
				}
			}

			for (int i = 0; i < curent_conv->x; i++)
			{
				for (int j = 0; j < curent_conv->y; j++)
				{
					if (curent_conv->data[i][j] != NULL)
						strcpy(temp[i][j], curent_conv->data[i][j]);
				}
			}

			for (int i = 0; i < curent_conv->x; i++)
			{
				for (int j = 0; j < curent_conv->y; i++)
				{
					free(curent_conv->data[i][j]);
				}
				free(curent_conv->data[i]);
				curent_conv->data[i] = temp[i];
			}
			curent_conv->y = new_size;
		}
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

		if (c == ' ' && i == 0)
		{
			c = getchar();
			continue;
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

		if (c == EOF)
		{
			if (i == 0)
			{
				break;
			}
			clearerr(stdin);
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
	// int pipe[2] = {0, 0};
	int i = 0;
	while (1)
	{
		word = data[i];
		if (word == NULL)
			break;
		if (strcmp(word, ">") == 0)
		{
			fd = open(data[i + 1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
			// dup2(STDOUT_FILENO, pipe[1]);
			// close(pipe[1]); // temp
			dup2(fd, STDOUT_FILENO);
			close(fd);
			data[i] = NULL;
		}
		if (strcmp(word, ">>") == 0)
		{
			fd = open(data[i + 1], O_CREAT | O_WRONLY | O_APPEND, 0666);
			// dup2(STDOUT_FILENO, pipe[1]);
			// close(pipe[1]);
			dup2(fd, STDOUT_FILENO);
			close(fd);
			data[i] = NULL;
		}
		if (strcmp(word, "<") == 0)
		{
			fd = open(data[i + 1], O_RDONLY);
			// dup2(STDIN_FILENO, pipe[0]);
			// close(pipe[0]);
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
	signal(SIGCHLD, SIG_DFL);
}

void execute_conv(struct conv *conv, struct group **group_head, struct node *node, int *the_end)
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
		execute_if_inner(conv, *group_head, &succes, the_end);
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

	while (1)
	{
		int status;
		pid_t wait_pid = waitpid(-(pid[0]), &status, WUNTRACED);
		if (wait_pid == -1)
		{
			tcsetpgrp(STDIN_FILENO, getpgrp());
			return;
		}
		if (WIFSTOPPED(status))
		{
			conv_group = creat_group();
			conv_group->size = conv_size;
			for (int i = 0; i < conv_size; i++)
			{
				insert_group_pr(&(conv_group->gr_pr), pid[i]);
			}
			conv_group->group_leader = conv_group->gr_pr->pid;

			struct group_pr *ptr = conv_group->gr_pr;
			while (1)
			{
				if (ptr->pid == wait_pid)
					break;
				ptr = ptr->next;
			}
			ptr->status = 2;

			insert_group(group_head, conv_group);
			node->block_and_flag = 0;
			node->block_or_flag = 0;
			tcsetpgrp(STDIN_FILENO, getpgrp());
			return;
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

void execute_if_inner(struct conv *conv, struct group *group_head, int *succes, int *the_end)
{
	if (strcmp(conv->data[0][0], "cd") == 0)
	{
		if (conv->data[0][1] == NULL)
		{
			printf("cd : Set apropriate path.\n");
			return;
		}
		int cd = chdir(conv->data[0][1]);
		if (cd == -1)
		{
			printf("cd : Set apropriate path.\n");
		}
		*succes = 1;
	}
	if (strcmp(conv->data[0][0], "quit") == 0)
	{
		*the_end = 1;
		*succes = 1;
	}
	if (strcmp(conv->data[0][0], "jobs") == 0)
	{
		if (conv->data[0][1] != NULL)
		{
			if (strcmp(conv->data[0][1], "-e"))
			{
				print_jobs_expanded(group_head);
				*succes = 1;
				return;
			}
		}
		print_jobs_expanded(group_head);
		*succes = 1;
	}
	if (strcmp(conv->data[0][0], "kill") == 0)
	{
		if (conv->data[0][1] == NULL)
		{
			printf("Enter command in kill <sig> <pid>/[-<pgid>]\n");
			*succes = 1;
			return;
		}

		if (conv->data[0][2] == NULL)
		{
			printf("Enter command in kill <sig> <pid>/[-<pgid>]\n");
			*succes = 1;
			return;
		}

		int work = kill((atoi(conv->data[0][2])), atoi(conv->data[0][1]));
		if (work == -1)
		{
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
		}
		errno = 0;
		*succes = 1;
	}
	if (strcmp(conv->data[0][0], "fg") == 0)
	{
		if (conv->data[0][1] == NULL)
		{
			printf("<fg> : Use fg <gpid>\n");
			return;
		}
		pid_t target_pid = atoi(conv->data[0][1]);
		return_foreground(target_pid, group_head);
		*succes = 1;
	}
	if (strcmp(conv->data[0][0], "bg") == 0)
	{
		if (conv->data[0][1] == NULL)
		{

			printf("<bg> : Use bg <gpid>.\n");
			return;
		}

		int work = kill(-(atoi(conv->data[0][1])), 18);
		if (work == -1)
		{
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
		}
		errno = 0;
		*succes = 1;
		return;
	}
}

void return_foreground(pid_t target_pid, struct group *group_head)
{
	while (1)
	{
		if (group_head->group_leader == target_pid)
		{
			break;
		}
		if (group_head == NULL)
		{
			printf("<fg> : enter correct pgid\n");
			return;
		}
		group_head = group_head->next;
	}

	if (group_head->status == 3 || group_head->status == 4 || group_head->status == 5)
	{
		printf("<fg> : enter correct pgid\n");
		return;
	}

	tcsetpgrp(STDIN_FILENO, group_head->group_leader);
	kill(-(target_pid), SIGCONT);

	group_head->status = 4;

	while (1)
	{
		int status;
		int wait_pid = waitpid(-(group_head->group_leader), &status, WUNTRACED);
		if (!WIFEXITED(status))
		{
			if (WIFSTOPPED(status))
			{
				tcsetpgrp(STDIN_FILENO, getpgrp());
				group_head->status = 2;
				return;
			}
		}
		if (wait_pid == -1)
		{
			tcsetpgrp(STDIN_FILENO, getpgrp());
			return;
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
		while (1)
		{
			int status = 0;
			pid_t wait_pid = waitpid(-(group_head->group_leader), &status, WUNTRACED | WNOHANG | WCONTINUED);
			if (wait_pid == 0)
			{
				break;
			}
			if (wait_pid == -1)
			{
				break;
			}
			if (wait_pid > 1)
			{
				struct group_pr *ptr = group_head->gr_pr;
				while (ptr != NULL)
				{

					if (ptr->pid == wait_pid)
						break;
					ptr = ptr->next;
				}

				if (WIFEXITED(status))
				{
					ptr->status = 3;
				}
				if (WIFSIGNALED(status))
				{
					if (WTERMSIG(status) == SIGKILL)
					{
						ptr->status = 4;
					}
				}
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
		group_head = group_head->next;
	}
}

void analyze_jobs(struct group *group_head)
{
	if (group_head == NULL)
	{
		return;
	}
	while (group_head != NULL)
	{
		if (group_head->status == 3 || group_head->status == 4 || group_head->status == 5)
		{
			group_head = group_head->next;
			continue;
		}
		int stop_c = 0;
		int exit_c = 0;
		int killed_c = 0;
		struct group_pr *ptr = group_head->gr_pr;
		while (ptr != NULL)
		{
			if (ptr->status == 2)
			{
				stop_c++;
			}
			if (ptr->status == 3)
			{
				exit_c++;
			}
			if (ptr->status == 4)
			{
				killed_c++;
			}
			ptr = ptr->next;
		}

		if (killed_c == group_head->size)
		{
			group_head->status = 5;
			break;
		}
		if (stop_c == group_head->size - killed_c)
		{
			group_head->status = 2;
		}
		if (exit_c == group_head->size - killed_c)
		{
			group_head->status = 3;
		}
		if (stop_c + exit_c < group_head->size - killed_c)
		{
			group_head->status = 1;
		}
		group_head = group_head->next;
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
			printf("[%d] Stopped status\n", group_head->group_leader);
		}
		if (group_head->status == 3)
		{
			printf("[%d] Finished\n", group_head->group_leader);
			group_head->status = 4;
		}
		if (group_head->status == 5)
		{
			printf("[%d] Killed\n", group_head->group_leader);
			group_head->status = 4;
		}
		group_head = group_head->next;
	}
}

void print_jobs_expanded(struct group *group_head)
{
	while (group_head != NULL)
	{
		if (group_head->status == 1)
		{
			printf("[%d] Working\n", group_head->group_leader);
			printf("  ↳\n");
			struct group_pr *ptr = group_head->gr_pr;
			while (ptr != NULL)
			{
				if (ptr->status == 1)
				{
					printf("   [%d]Working\n", ptr->pid);
				}
				if (ptr->status == 2)
				{
					printf("   [%d]Stopped\n", ptr->pid);
				}
				if (ptr->status == 3)
				{
					printf("   [%d]Ended\n", ptr->pid);
				}
				if (ptr->status == 4)
				{
					printf("   [%d]Killed\n", ptr->pid);
				}
				ptr = ptr->next;
			}
		}
		if (group_head->status == 2)
		{
			printf("[%d] Stopped status\n", group_head->group_leader);
			printf("  ↳\n");
			struct group_pr *ptr = group_head->gr_pr;
			while (ptr != NULL)
			{
				if (ptr->status == 1)
				{
					printf("   [%d]Working\n", ptr->pid);
				}
				if (ptr->status == 2)
				{
					printf("   [%d]Stopped\n", ptr->pid);
				}
				if (ptr->status == 3)
				{
					printf("   [%d]Ended\n", ptr->pid);
				}
				if (ptr->status == 4)
				{
					printf("   [%d]Killed\n", ptr->pid);
				}
				ptr = ptr->next;
			}
		}
		if (group_head->status == 3)
		{
			printf("[%d] Finished\n", group_head->group_leader);
			group_head->status = 4;
		}
		if (group_head->status == 5)
		{
			printf("[%d] Killed\n", group_head->group_leader);
			group_head->status = 4;
		}
		group_head = group_head->next;
	}
}

void execute(struct node *head, struct group **group, int *the_end)
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
		execute_conv(cur_conv, group, head, the_end);
	}
}

void data_destroyer(struct node **head, struct group **group)
{
	destroy_node(*head);
	destroy_group(*group);
}

void destroy_node(struct node *nd)
{
	while (nd != NULL)
	{
		struct conv *current_conv = nd->head;
		while (current_conv != NULL)
		{
			for (int i = 0; i < current_conv->x; i++)
			{
				for (int j = 0; j < current_conv->y; j++)
				{
					free(current_conv->data[i][j]);
					current_conv->data[i][j] = NULL;
				}
				free(current_conv->data[i]);
				current_conv->data[i] = NULL;
			}
			free(current_conv->data);
			free(current_conv->start_flag);
			struct conv *temp_conv = current_conv;
			current_conv = current_conv->next;
			free(temp_conv);
		}
		struct node *temp_nd = nd;
		nd = nd->next;
		free(temp_nd);
	}
}

void destroy_group(struct group *grp)
{
	while (grp != NULL)
	{
		struct group_pr *current_pr = grp->gr_pr;
		while (current_pr != NULL)
		{
			struct group_pr *temp_pr = current_pr;
			current_pr = current_pr->next;
			free(temp_pr);
		}
		struct group *temp_grp = grp;
		grp = grp->next;
		free(temp_grp);
	}
}

int main()
{
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	setpgid(getpid(), getpid());
	tcsetpgrp(STDIN_FILENO, getpgrp());

	struct node *head = NULL;
	struct group *group_head = NULL;
	char *ptr;
	int the_end = 0;
	while (1)
	{
		jobs_control(group_head);
		analyze_jobs(group_head);
		printf("~shell: ");
		ptr = read_func();
		if (ptr == NULL)
		{
			if (feof(stdin))
			{
				data_destroyer(&head, &group_head);
				break;
			}
			continue;
		}
		insert_command(&head, ptr);
		execute(head, &group_head, &the_end);
		if (the_end)
		{
			data_destroyer(&head, &group_head);
			break;
		}
		jobs_control(group_head);
		analyze_jobs(group_head);
	}
	puts("exit");
	return 0;
}
// verified