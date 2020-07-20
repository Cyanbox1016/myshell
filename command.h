#pragma once
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>
#include <map>
#include <vector>
#include <cstdio>
#define MAX_PATH_LEN 256
#define MAX_WORD_LEN 128

class job
{
public:
	pid_t pid;
	pid_t pgid;
	// pid_t sid;
	pid_t jobid;
	char** argv;

	int status;
	std::string state;
	job* last;
	job* next;

	job()
	{
		pid = -1;
		pgid = -1;
		jobid = -1;
		// sid = -1;
		last = NULL;
		next = NULL;
	}

	~job() {}

	void print_job();
};

class job_list
{
public:
	static job *head;
	static job *tail;

	~job_list() {}

	void add_job(job*);
	void del_job(job*);

	int job_check();
};

extern bool halt;
extern char working_directory[MAX_PATH_LEN];
extern char temp_path[MAX_PATH_LEN];
extern enum Command_state
{
	STATE_HALT,	
	STATE_PWD,
	STATE_COMMAND,
	STATE_CLEARSCREEN,
	STATE_CD,
	STATE_ENV,
	STATE_ECHO,
	STATE_DIR,
	STATE_TIME,
	STATE_EXEC,
	STATE_UMASK,
	STATE_FG
} command_state;

extern std::map<std::string, Command_state> c_map;  
extern FILE *file_in, *file_out;
extern char* shell_path, *shell_dir;
extern char* parameter[10];
extern int f_stdin, f_stdout;
extern job_list *j_list;

void init_map();
void c_interpret(std::string);
void c_exec(std::vector<std::string> c_word);
void clear_pipe();
void init_shell();
