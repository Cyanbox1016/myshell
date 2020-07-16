#include <unistd.h>
#include <stdio.h>
#include <cstring>
#include "command.h"

#define MAX_USR_LEN 128

char user_name[MAX_USR_LEN];
char domain_name[MAX_USR_LEN];

int main(int argc, char** argv)
{	
	init_map();
	clear_pipe();
	
	chdir(getenv("HOME"));

	while (!halt) 
	{
		cuserid(user_name);
		gethostname(domain_name, MAX_USR_LEN);
		getcwd(working_directory, MAX_PATH_LEN);
		char* pos = strstr(working_directory, getenv("HOME"));
		if (pos)
		{
			char path_buf[MAX_PATH_LEN] = "~";
			strcat(path_buf, pos + strlen(getenv("HOME")));
			strcpy(working_directory, path_buf);
		} 
		printf("\033[36m%s@%s\033[0m", user_name, domain_name);
		printf(":");
		printf("\033[35m%s\033[0m", working_directory);
		printf("$ ");
		c_interpret();
	}
	return 0;
}