#pragma once
#include <string>
#include <map>
#include <vector>
#include <cstdio>
#define MAX_PATH_LEN 256
#define MAX_WORD_LEN 128

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
	STATE_EXEC
} command_state;

extern std::map<std::string, Command_state> c_map;  
extern FILE *file_in, *file_out;

void init_map();
void c_interpret();
void c_exec(std::vector<std::string> c_word);
void clear_pipe();
