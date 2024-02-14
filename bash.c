#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

struct conv
{
	int commands_count;
	char *start_flag;
	char ***data;
	struct conv *next;
};

struct node
{
	int bg_flag;
	int conv_count;
	struct conv *head;
	struct node *next;
};

struct group
{
	pid_t group_leader;
	int size;
	pid_t *slaves;
	struct group *next;
};

struct group *creat_group(int i)
{
	struct group *ptr = malloc(sizeof(struct group));
	ptr->size = i;
	ptr->slaves = malloc(sizeof(pid_t) * ptr->size);
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
	ptr->next = NULL;
	return ptr;
}

struct node *create_node(char *command)
{
	struct node *ptr = calloc(1, sizeof(struct node));
	ptr->conv_count = 1;
	ptr->bg_flag = 0;
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
		if (strcmp(part, "||") == 0 || strcmp(part, "&&") == 0 || strcmp(part, ";") == 0)
		{
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
	printf("\e[1;96m");
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
	printf("\e[m");
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
			fd = open(data[i + 1], O_CREAT | O_WRONLY | O_EXCL, 0666);
			dup2(STDOUT_FILENO, pipe[1]);
			close(pipe[1]);
			dup2(fd, STDOUT_FILENO);
			close(fd);
			data[i] = NULL;
		}
		if (strcmp(word, "<") == 0)
		{
			fd = open(data[i + 1], O_CREAT, O_RDWR | 0666);
			dup2(STDIN_FILENO, pipe[0]);
			close(pipe[0]);
			dup2(fd, STDIN_FILENO);
			close(fd);
			data[i] = NULL;
		}
		i++;
	}
}

void execute_conv(struct conv *conv)
{
	int commands_num = conv->commands_count;
	if (commands_num == 1) // простой вариант играем от одного процесса
	{
		pid_t pid_procc = 0;
		pid_procc = fork();
		if (pid_procc == 0)
		{
			file_redirecting(conv->data[0]);
			execvp(conv->data[0][0], conv->data[0]);
			_exit(200);
		}
	}
	if (commands_num == 2) // продвинутый вариант, от двух процессов
	{
		int fd[2];
		pipe(fd);
		pid_t pid1 = 0, pid2 = 0;
		pid1 = fork();
		if (pid1 == 0)
		{
			dup2(fd[1], STDOUT_FILENO);
			close(fd[0]);
			close(fd[1]);
			file_redirecting(conv->data[0]);
			execvp(conv->data[0][0], conv->data[0]);
			_exit(200);
		}
		pid2 = fork();
		if (pid2 == 0)
		{
			dup2(fd[0], STDIN_FILENO);
			close(fd[0]);
			close(fd[1]);
			file_redirecting(conv->data[1]);
			execvp(conv->data[1][0], conv->data[1]);
		}
		close(fd[0]);
		close(fd[1]);
	}
	if (commands_num > 2) // гроб вариант от трех процессов
	{
		int fd[commands_num - 1][2];
		for (int i = 0; i < commands_num - 1; i++)
		{
			pipe(fd[i]);
		}

		pid_t conv_pids[commands_num];
		for (int i = 0; i < commands_num; i++)
		{
			conv_pids[i] = 0;
		}

		for (int i = 0; i < commands_num; i++)
		{
			conv_pids[i] = fork();
			if (conv_pids[i] == 0)
			{
				if (i == 0)
				{
					dup2(fd[i][1], STDOUT_FILENO);
					close(fd[i][0]);
					close(fd[i][1]);
					for (int j = 1; j < commands_num - 1; j++)
					{
						close(fd[j][0]);
						close(fd[j][1]);
					}
					file_redirecting(conv->data[i]);
					execvp(conv->data[i][0], conv->data[i]);
					_exit(200);
				}
				if (i == commands_num - 1)
				{
					dup2(fd[i - 1][0], STDIN_FILENO);
					close(fd[i - 1][0]);
					close(fd[i - 1][1]);
					for (int j = 0; j < commands_num - 1; j++)
					{
						close(fd[j][0]);
						close(fd[j][1]);
					}
					file_redirecting(conv->data[i]);
					execvp(conv->data[i][0], conv->data[i]);
					_exit(200);
				}
				if (i > 0)
				{
					for (int j = 0; j < i - 1; j++)
					{
						close(fd[j][0]);
						close(fd[j][1]);
					}
					for (int j = i + 1; j < commands_num - 1; j++)
					{
						close(fd[j][0]);
						close(fd[j][1]);
					}
					dup2(fd[i][1], STDOUT_FILENO);
					dup2(fd[i - 1][0], STDIN_FILENO);
					close(fd[i][0]);
					close(fd[i][1]);
					close(fd[i - 1][0]);
					close(fd[i - 1][1]);
					file_redirecting(conv->data[i]);
					execvp(conv->data[i][0], conv->data[i]);
					_exit(200);
				}
			}
			if (i > 0 && i < commands_num - 1)
			{
				close(fd[i - 1][0]);
				close(fd[i - 1][1]);
			}
			if (i == commands_num - 1)
			{
				close(fd[i - 1][0]);
				close(fd[i - 1][1]);
			}
		}
	}
}

void execute(struct node *head)
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
		execute_conv(cur_conv);
	}
}

int main()
{
	struct node *head;
	head = NULL;
	char *ptr;
	while (1)
	{
		printf("\e[1;33m > \e[m \n");
		ptr = read_func();
		if (ptr == NULL)
			continue;
		insert_command(&head, ptr);
		execute(head);
	}
	return 0;
}
