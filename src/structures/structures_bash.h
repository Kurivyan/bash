struct command_line {
	struct conveyor* conveyer_head;
	struct command_line* next
};

struct group{
	struct command_line* command_line_head;
	int in_charge;
	struct group* next;
};

struct jobs{
	int status;
	struct jobs* next;
};

struct conveyor {
	int command_counter;
	struct command *command_node;
	struct conveyor *next;
};

struct command{
	char* comamnd_name;
	char** argiments;
	int argiments_counter;
	int input_file;
	int output_file;
	struct command* next;
};

#include "structures_bash.c"