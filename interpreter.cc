// 文件名：interpreter.cc
// 作者：3180105481 王泳淇
// 功能：用户交互界面，包含myshell的入口
#include <unistd.h>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include "command.h"

#define MAX_USR_LEN 128

using std::string;
using std::cin;
using std::cout;
using std::ifstream;

// user_name：存储当前登录的用户名
char user_name[MAX_USR_LEN];
// domain_name：存储当前登录的主机名
char domain_name[MAX_USR_LEN];
// parameter：命令行参数表
const char* parameter[10];

int main(int argc, char** argv)
{	
	// 调用init_shell，init_map, init_test和clear_pipe函数，进行myshell启动的初始化工作
	init_shell();
	init_map();
	init_test();
	clear_pipe();
	
	// 设定默认工作目录为主目录，并修改对应的环境变量
	chdir(getenv("HOME"));
	setenv("PWD", getenv("HOME"), 1);

	// 初始化命令行参数到parameter数组
	if (!argv[1])
	{
		parameter[0] = "myshell";
		for (int i = 1; i < 10; i++)
		{
			parameter[i] = NULL;
		}
	}
	else
	{
		parameter[0] = "myshell";
		for (int i = 1; i < 10; i++)
		{
			if (i < argc)
			{
				parameter[i] = argv[i];
			}
			else
			{
				parameter[i] = NULL;
			}
		}
	}

	// 停机前持续循环
	while (!halt) 
	{
		// 获得用户名、主机名和工作目录
		cuserid(user_name);
		gethostname(domain_name, MAX_USR_LEN);
		getcwd(working_directory, MAX_PATH_LEN);
		// 若working_directory中出现主目录路径，则将其替换为~符号
		char* pos = strstr(working_directory, getenv("HOME"));
		if (pos)
		{
			char path_buf[MAX_PATH_LEN] = "~";
			strcat(path_buf, pos + strlen(getenv("HOME")));
			strcpy(working_directory, path_buf);
		} 
		// 从终端读取命令模式
		if (!argv[1])
		{
			// 在if语句内，输出二级提示符
			if (if_state != NO_IF)
			{
				printf("> ");
			}
			// 输入用户名、主机名、当前工作目录和提示符$
			else
			{
				printf("\033[36m%s@%s\033[0m", user_name, domain_name);
				printf(":");
				printf("\033[35m%s\033[0m", working_directory);
				printf("$ ");
			}
			
			string command_in;
			std::vector<string> commands;
			commands.clear();
			// 读入命令行
			getline(cin, command_in);
			// 将一行命令中用;分隔开的命令分开成多条指令，放在commands中
			split_line(commands, command_in, ";");
			// 逐个解析commands中的指令
			for (std::vector<string>::iterator i = commands.begin(); i != commands.end(); i++)
			{
				c_interpret(*i);
			}
		}
		// 从脚本中读取命令
		else
		{
			// 打开脚本文件
			ifstream command_file(argv[1]);
			if (command_file.fail())
			{
				printf("%s: No such file or directory\n", argv[1]);
				exit(0);
			}
		
			string command_in;
			std::vector<string> commands;
			while (!command_file.eof())
			{
				// 从脚本中逐行获得命令行
				getline(command_file, command_in);
				commands.clear();
				// 将一行命令中用;分隔开的命令分开成多条指令，放在commands中
				split_line(commands, command_in, ";");
				// 逐个解析commands中的指令
				for (std::vector<string>::iterator i = commands.begin(); i != commands.end(); i++)
				{
					c_interpret(*i);
				}
			}
			// 执行完毕，退出myshell
			exit(0);
		}
		
	}
	return 0;
}