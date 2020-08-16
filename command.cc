// 文件名：command.cc
// 作者：3180105481 王泳淇
// 功能：myshell使用的核心函数的具体实现

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "command.h"

using std::cin;
using std::cout;
using std::vector;
using std::string;
using std::stringstream;
using std::endl;
using std::map;

// ----------------与命令执行相关的全局变量----------------

// halt：标志是否停机，当halt为false时myshell持续运行
bool halt = false;
// bg：表示是否在后台运行当前命令，当值为true时在后台运行命令
bool bg = false;
// working_directory：当前工作目录
char working_directory[MAX_PATH_LEN];
// command_state：根据输入命令决定的命令执行状态，不同状态决定着不同的后续操作
enum Command_state command_state;
// c_map：用于将输入的命令映射到相应的command_state
map<string, Command_state> c_map;
// file_in, file_out：指向输入输出文件的FILE*指针
FILE *file_in, *file_out;
// f_stdin, f_stdout：用于备份标准输入输出的文件号
int f_stdin = -1, f_stdout = -1;
// rein, reout, reappend：用于记录是否发生重定向（不包括管道）
bool rein = false, reout = false, reappend = false;
// infile, outfile：用于备份重定向文件名
string infile, outfile;

// ----------------与环境变量相关的系统变量----------------

// environ：外部全局变量，用于获取环境变量
extern char** environ;
// envir：环境变量列表指针
char** envir;
// shell_path：myshell的路径
// shell_dir：myshell所在的目录
char* shell_path, *shell_dir;
// pipe_in_path, pipe_out_path：管道文件的路径
char* pipe_in_path, *pipe_out_path;

// ----------------与后台作业相关的全局变量----------------

// j_list：后台作业链表，为双向链表
job_list *j_list;
// head, tail：后台作业链表的头和尾
job* job_list::head = NULL;
job* job_list::tail = NULL;

// ----------------与if语句相关的全局变量----------------

// if_state：标志当前运行到if语句的那个状态
// 枚举的元素及含义见command.h
enum If_state if_state = NO_IF;
// if_buffer：用于存放if语句内部的命令
std::vector<std::string> if_buffer;
enum Test_expr test_expr;
std::map<std::string, Test_expr> test_map;

//  ----------------与用户定义变量相关的全局变量----------------
// var_num变量表示用户定义的变量数目
int var_num = 0;
// var_map：用于将变量名映射到变量列表下标
std::map<std::string, int> var_map;
// var_list：存放变量的vector
std::vector<std::string> var_list;

// ----------------字符串处理相关函数，主要用于语句解析----------------

// 函数：trim_string
// 功能：去掉字符串两边的空白字符
// 输入：待去空白字符的const string& 型变量
// 返回：两边无空白字符的string变量
string trim_string (const string& str)
{
	string s = str;

	// 空白字符列表
	string blanks("\f\v\r\t\n ");
	// 去掉前面的空白字符
	s.erase(0, s.find_first_not_of(blanks));
	// 去掉后面的空白字符
	s.erase(s.find_last_not_of(blanks) + 1);

	return s;
}

// 函数：split_line
// 功能：将输入的字符串用指定的字符分割成多个子字符串
// 输入：存放子字符串的vector word，待分割字符串line，可指定的分割字符列表splitter，默认值为空白字符列表
// 返回：无
inline void split_line(vector<string>& words, string line, string splitter = "\f\v\r\t\n ")
{
	size_t pos;
	
	// 清空words
	words.clear();
	// 寻找下一个指定字符，并将指定字符之前的子串trim后放入words中
	while ((pos = line.find_first_of(splitter)) != line.npos)
	{
		words.push_back(line.substr(0, line.find_first_of(splitter)));
		line.erase(0, line.find_first_of(splitter) + 1);
		line = trim_string(line);
	}
	// 放入最后剩下的子串
	words.push_back(line);
}

// 函数：var_translate
// 功能：解析命令语句中的变量
// 输入：命令行参数列表words
// 返回：无
void var_translate(vector<string>& words)
{
	// 遍历命令行参数包含的vector，寻找是否有以$开头的参数
	for (vector<string>::iterator i = words.begin(); i != words.end(); i++)
	{
		if ((*i).find("$") == 0)
		{
			// 尝试将变量名转化为数字
			// 若变量是数字且在0~9范围内，则作为命令行参数解析。仅支持参数0~9，范围超出则将变量解析为空串
			try
			{
				int num = std::stoi((*i).substr(1, (*i).length() - 1));
				if (num < 0 || num > 9)
					*i = "";
				else if (parameter[num] == NULL)
					*i = "";
				else
					*i = parameter[num];
					
			}
			// 变量名无法解析为数字，则作为环境变量或用户定义的变量解析，其中优先将变量解析为用户定义的变量
			catch(const std::invalid_argument& e)
			{
				// 去掉首位的$
				string var_name = (*i).substr(1, (*i).length() - 1);
				// 当前变量名没有定义过，则尝试解析为环境变量
				if (var_map.find(var_name) == var_map.end())
				{
					// 找到相应的环境变量，带入环境变量的值
					if (getenv(var_name.c_str()))
					{
						*i = getenv(var_name.c_str());
					}
					// 没找到相应的环境变量，带入空值
					else
					{
						*i = "";
					}
				}
				// 将存储的变量值代入原位置
				else
				{
					*i = var_list[var_map[var_name]];
				}
			}
		}
	}
}

// 函数：format_path
// 功能：将路径中主目录的其他表示形式翻译成主目录的绝对路径
// 输入：含主目录相对路径的路径path
// 返回：包含主目录绝对路径的路径
inline string format_path(const string& path)
{
	string ans = path;

	// 翻译‘~’符号
	while (ans.find("~") != ans.npos) 
	{
		ans.replace(ans.find("~"), 1, getenv("HOME"));
	}
	// 翻译环境变量HOME
	while (ans.find("$HOME") != ans.npos)
	{
		ans.replace(ans.find("$HOME"), 5, getenv("HOME"));
	}

	return ans;
}

// ----------------myshell初始化使用的相关函数----------------

// 函数：init_shell()
// 功能：进行一些shell初始化工作，主要包括获取、设置环境变量和建立后台作业链表
// 输入：无
// 返回：无
void init_shell()
{
	shell_path = (char*)malloc(MAX_PATH_LEN);
	shell_dir = (char*)malloc(MAX_PATH_LEN);

	// 获得myshell文件的路径，并将其赋值给SHELL变量
	readlink("/proc/self/exe", shell_path, MAX_PATH_LEN - 1);
	setenv("SHELL", shell_path, 1);

	// 获得myshell文件所在目录的路径
	strcpy(shell_dir, shell_path);
	int len = strlen(shell_dir);
	for (int i = len - 1; i >= 0; i--)
	{
		if (shell_dir[i] == '/')
		{
			shell_dir[i] = '\0';
			break;
		}
	}
	
	// 建立管道文件，并存储管道文件的目录
	pipe_in_path = (char*)malloc(MAX_PATH_LEN);
	pipe_out_path = (char*)malloc(MAX_PATH_LEN);
	strcpy(pipe_in_path, shell_dir);
	strcpy(pipe_out_path, shell_dir);
	strcat(pipe_in_path, "pipe_in");
	strcat(pipe_out_path, "pipe_out");

	// 将shell_dir加入环境变量PATH
	string env_path = getenv("PATH");
	env_path += ":";
	env_path += shell_dir;
	setenv("PATH", env_path.c_str(), 1);

	// 建立后台作业链表j_list
	j_list = new job_list;
	
}

// 函数：init_map
// 功能：初始化c_map，将指令类型和执行指令的状态的键值对插入到c_map中
// 输入：无
// 返回：无
void init_map() 
{
	c_map.insert(std::pair<string, Command_state>("exit", STATE_HALT));
	c_map.insert(std::pair<string, Command_state>("quit", STATE_HALT));
	c_map.insert(std::pair<string, Command_state>("pwd", STATE_PWD));
	c_map.insert(std::pair<string, Command_state>("clr", STATE_CLEARSCREEN));
	c_map.insert(std::pair<string, Command_state>("clear", STATE_CLEARSCREEN));
	c_map.insert(std::pair<string, Command_state>("cd", STATE_CD));
	c_map.insert(std::pair<string, Command_state>("environ", STATE_ENV));
	c_map.insert(std::pair<string, Command_state>("echo", STATE_ECHO));
	c_map.insert(std::pair<string, Command_state>("dir", STATE_DIR));
	c_map.insert(std::pair<string, Command_state>("time", STATE_TIME));
	c_map.insert(std::pair<string, Command_state>("exec", STATE_EXEC));
	c_map.insert(std::pair<string, Command_state>("umask", STATE_UMASK));
	c_map.insert(std::pair<string, Command_state>("fg", STATE_FG));
	c_map.insert(std::pair<string, Command_state>("bg", STATE_BG));
	c_map.insert(std::pair<string, Command_state>("jobs", STATE_JOBS));
	c_map.insert(std::pair<string, Command_state>("shift", STATE_SHIFT));
	c_map.insert(std::pair<string, Command_state>("set", STATE_SET));
	c_map.insert(std::pair<string, Command_state>("if", STATE_IF));
	c_map.insert(std::pair<string, Command_state>("then", STATE_THEN));
	c_map.insert(std::pair<string, Command_state>("fi", STATE_FI));
	c_map.insert(std::pair<string, Command_state>("unset", STATE_UNSET));
	c_map.insert(std::pair<string, Command_state>("help", STATE_HELP));
}

// 函数：init_test
// 功能：初始化test_map，将符号和表达式类型的键值对插入到test_map中
// 输入：无
// 返回：无
void init_test()
{
	test_map.insert(std::pair<string, Test_expr>("!=", STR_NE));
	test_map.insert(std::pair<string, Test_expr>("==", STR_EQ));
	test_map.insert(std::pair<string, Test_expr>("=", STR_EQ));
	test_map.insert(std::pair<string, Test_expr>("\\<", STR_LE));
	test_map.insert(std::pair<string, Test_expr>("\\>", STR_GR));
	test_map.insert(std::pair<string, Test_expr>("-n", STR_NONZERO));
	test_map.insert(std::pair<string, Test_expr>("-z", STR_ZERO));
	test_map.insert(std::pair<string, Test_expr>("-f", FILE_F));
	test_map.insert(std::pair<string, Test_expr>("-r", FILE_R));
	test_map.insert(std::pair<string, Test_expr>("-w", FILE_W));
	test_map.insert(std::pair<string, Test_expr>("-x", FILE_E));
	test_map.insert(std::pair<string, Test_expr>("-d", DIR_F));
	test_map.insert(std::pair<string, Test_expr>("-eq", INT_EQ));
	test_map.insert(std::pair<string, Test_expr>("-ge", INT_GE));
	test_map.insert(std::pair<string, Test_expr>("-gt", INT_GT));
	test_map.insert(std::pair<string, Test_expr>("-le", INT_LE));
	test_map.insert(std::pair<string, Test_expr>("-lt", INT_LT));
	test_map.insert(std::pair<string, Test_expr>("-ne", INT_NE));
}

// 函数：clear_pipe
// 功能：清除管道文件内容，去除上次使用管道遗留下来的内容
// 输入：无
// 返回：无
void clear_pipe()
{
	fclose(fopen(pipe_out_path, "w"));

	fclose(fopen(pipe_in_path, "w"));
}

// 函数：sync_pipe
// 功能：用于管道文件的同步，每次运行命令前上一级管道输出作为下一级管道输入，并清空上一级管道输出
// 输入：无
// 返回：表示同步是否成功的代码，0为成功，-1为失败
inline int sync_pipe()
{	
	// 清空pipe_in
	fclose(fopen(pipe_in_path, "w"));

	int pipe_in, pipe_out;
	char buf[1024];
	size_t nread;

	// 打开管道文件，若文件打开失败则返回-1
	pipe_in = open(pipe_in_path, O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
	pipe_out = open(pipe_out_path, O_RDONLY);
	if (pipe_in < 0 || pipe_out < 0)
		return -1;
	
	// 将内容从pipe_out拷贝到pipe_in
	while (nread = read(pipe_out, buf, sizeof(buf)), nread > 0)
	{
		char* out = buf;
		size_t nwritten;

		do
		{
			nwritten = write(pipe_in, out, nread);
			if (nwritten >= 0)
			{
				nread -= nwritten;
				out += nwritten;
			}
		} while (nread > 0);
	}

	close(pipe_in);
	close(pipe_out);

	// 清空pipe_out
	fclose(fopen(pipe_out_path, "w"));

	return 0;
}

// ----------------表达式真值判定函数----------------

// 函数：judge_expression
// 功能：用于测试表达式真值（即实现test命令）
// 输入：以test起始的表达式值
// 返回：表达式真值
bool judge_expression(vector<string> c_word)
{
	// 不支持不以test起始的表达式
	if (c_word[0] != "test")
	{
		printf("Syntax error with if clause: \"test\" missed\n");
		return false;
	}
	// 形如 test <变量1> <运算符> <变量2> 的表达式
	if (c_word.size() > 3)
	{
		// 不支持的运算符返回false
		if (test_map.find(c_word[2]) == test_map.end())
		{
			printf("Syntax error with if clause: invalid argument\n");
			return false;
		}
		else
		{
			test_expr=test_map[c_word[2]];
			int int1 = 0, int2 = 0;
			// 用于比较整数的运算符（参数），将变量转化为整数类型
			if (test_expr >= INT_EQ && test_expr <= INT_NE)
			{
				try
				{
					int1 = stoi(c_word[1]);
					int2 = stoi(c_word[3]);
				}
				// 整数运算符与字符串变量不匹配
				catch(const std::invalid_argument& e)
				{
					printf("ERROR: Operator not available for non-integer parameters\n");
					return false;
				}
				
			}
			switch (test_expr)
			{
				// 比较运算符!=，字符串不相等
				case STR_NE:
				{
					return c_word[1] == c_word[3] ? false : true;
				}
				// == 和 =，字符串相等
				case STR_EQ:
				{
					return c_word[1] == c_word[3] ? true : false;
				}
				// /<，字符串小于
				case STR_LE:
				{
					return c_word[1] < c_word[3] ? true : false;
				}
				// />，字符串大于
				case STR_GR:
				{
					return c_word[1] > c_word[3] ? true : false;
				}
				// -eq，整数相等
				case INT_EQ:
				{
					return int1 == int2 ? true : false;
				}
				// -ge，整数大于等于
				case INT_GE:
				{
					return int1 >= int2 ? true : false;
				}
				// -gt，整数大于
				case INT_GT:
				{
					return int1 > int2 ? true : false;
				}
				// -le，整数小于等于
				case INT_LE:
				{
					return int1 <= int2 ? true : false;
				}
				// -lt，整数小于
				case INT_LT:
				{
					return int1 < int2 ? true : false;
				}
				// -ne，证书不等
				case INT_NE:
				{
					return int1 != int2 ? true : false;
				}
				// 不支持的其他情况
				default:
				{
					printf("Syntax error with if clause: invalid argument\n");
					return false;
				}
			}
		}
		
	}
	// 形如 test <参数> <变量> 的表达式
	else if (c_word.size() == 3)
	{
		// 不支持的参数类型返回false
		if (test_map.find(c_word[1]) == test_map.end())
		{
			printf("Syntax error with if clause: invalid argument\n");
			return false;
		}
		else
		{
			test_expr=test_map[c_word[1]];
			
			switch (test_expr)
			{
				// -n 字符串长度不为0
				case STR_NONZERO:
				{
					return (c_word[2].length() > 0) ? true : false;
				}
				// -z 字符串长度为0
				case STR_ZERO:
				{
					return (c_word[2].length() > 0) ? false : true;
				}
				// -f 文件存在
				case FILE_F:
				{
					return (access(c_word[2].c_str(), F_OK) == 0) ? true : false;
				}
				// -x 文件可执行
				case FILE_E:
				{
					return (access(c_word[2].c_str(), F_OK | X_OK) == 0) ? true : false;
				}
				// -w 文件可写
				case FILE_W:
				{
					return (access(c_word[2].c_str(), F_OK | W_OK) == 0) ? true : false;
				}
				// -r 文件只读
				case FILE_R:
				{
					int read = access(c_word[2].c_str(), F_OK | R_OK);
					int write =  access(c_word[2].c_str(), F_OK | W_OK);
					return (read == 0 && write == -1) ? true : false;
				}
				// -d 目录存在
				case DIR_F:
				{
					DIR* dir = opendir(c_word[2].c_str());
					bool ans = (!dir) ? false : true;
					closedir(dir);
					return ans;
				}
				// 不支持的其他情况
				default:
				{
					printf("Syntax error with if clause: invalid argument\n");
					return false;
				}
			}
		}
	}
	// test str的情况
	else if (c_word.size() == 2)
	{
		// 当str长度不为0时返回true，排除参数为-n的情况
		if (c_word[1] == "-n") return false;
		return (c_word[1].length() > 0) ? true : false;
	}
	// 其他不支持的情况一律返回false
	else
	{
		printf("Syntax error with if clause: invalid argument\n");
		return false;
	}
}

// ----------------myshell核心函数----------------

// 函数：c_interpret
// 功能：解析输入的命令行，判定命令类型，并将处理结果进一步
// 		交给c_exec执行
// 输入：命令行字符串c_line
// 返回：无
void c_interpret(string c_line)
{
	// 读到EOF时退出myshell
	if (cin.eof())
	{
		printf("exit\n");
		exit(0);
	}

	// 读到空命令行，仅检查后台命令执行情况，不做其他工作
	if (c_line == "")
	{
		j_list->job_check();
		return;
	}

	// 如果当前命令行在if语句中且if条件为真，将这些命令行存储起来集中执行
	if (if_state == THEN_TRUE && c_line.find("fi") != 0)
	{
		if_buffer.push_back(c_line);
		return;
	}
	// 如果if条件为假则忽略这些语句
	else if (if_state == THEN_FALSE && c_line.find("fi") != 0)
	{
		return;
	}

	size_t pos = 0;
	vector<string> c_command;
	vector<string> c_word;

	// 若存在管道连接的命令，将管道之间的语句分开
	while ((pos = c_line.find("|")) != c_line.npos)
	{	
		c_command.push_back(trim_string(c_line.substr(0, (int)pos)));
		c_line.erase(0, pos + 1);
	}
	c_command.push_back(trim_string(c_line));

	// 分条解析命令
	for (vector<string>::iterator i = c_command.begin(); i != c_command.end(); i++)
	{
		bool in_redir = false;
		bg = false;
		vector<string> redir_word;

		// 将命令行分解为参数列表
		split_line(c_word, *i);

		// 将参数中的相对路径修改为绝对路径
		for (vector<string>::iterator i = c_word.begin(); i != c_word.end(); i++)
		{
			*i = format_path(*i);
		}

		// 将参数中的变量引用替换为变量的值
		var_translate(c_word);
		
		// 根据c_command中的命令数目和当前命令的位置，确定输入和输出采用标准输入输出还是管道文件
		file_in = stdin;
		if (c_command.size() != 1)
		{
			// 管道连接命令的第一条，输出到pipe_out，从stdin输入
			if (i == c_command.begin())
			{
				// 备份标准输出
				f_stdout = dup2(1, 8);
				// 打开管道文件
				int fout = open(pipe_out_path, O_WRONLY|O_CREAT|O_TRUNC);
				if (fout == -1)
				{
					printf("ERROR: pipe buffer not available\n");
					return;
				}
				// 用管道文件替换标准输出
				dup2(fout, 1);
				close(fout);
			}
			// 管道连接命令的最后一条，从pipe_in输入，输出到stdout
			else if (i == c_command.end() - 1)
			{	
				// 打开管道文件（文件流方法）
				file_in = fopen(pipe_in_path, "r");
				if (!file_in)
				{
					printf("ERROR: pipe buffer not available\n");
					return;
				}
				in_redir = true;

				// 备份标准输入文件
				f_stdin = dup2(0, 7);
				// 打开管道文件
				int fin = open(pipe_in_path, O_RDONLY);
				if (fin == -1)
				{
					printf("ERROR: pipe buffer not available\n");
					return;
				}
				// 使用管道文件替换标准输入
				dup2(fin, 0);
				close(fin);
			}
			// 多条管道连接命令的中间，从管道输入，并输出到管道
			else
			{
				// 打开管道文件（文件流格式）
				file_in = fopen(pipe_in_path, "r");
				if (!file_in)
				{
					printf("ERROR: pipe buffer not available");
					return;
				}
				// 备份标准输入
				f_stdin = dup2(0, 7);
				// 打开管道文件
				int fin = open(pipe_in_path, O_RDONLY);
				if (fin == -1)
				{
					printf("ERROR: pipe buffer not available\n");
					return;
				}
				// 使用管道文件替换标准输入
				dup2(fin, 0);
				close(fin);

				// 备份标准输出文件
				f_stdout = dup2(1, 8);
				// 打开管道文件
				int fout = open(pipe_out_path, O_WRONLY|O_CREAT|O_TRUNC);
				if (fout == -1)
				{
					printf("ERROR: pipe buffer not available\n");
					return;
				}
				// 用管道文件替换标准输出
				dup2(fout, 1);
				close(fout);

				in_redir = true;
			}
		}

		// 扫描语句中的重定向符号和后台执行符号
		for (vector<string>::iterator j = c_word.begin(); j != c_word.end(); j++)
		{
			// 扫描到重定向输入符号，将输入从stdin修改到文件
			if (*j == "<")
			{
				const char* path_in = (*(j+1)).c_str();
				// 打开输入文件（文件流格式）
				file_in = fopen(path_in, "r");
				if (!file_in)
				{
					printf("%s: invalid file or directory\n", path_in);
					return;
				}
				// 标记：发生输入重定向，并备份文件名
				in_redir = true;
				rein = true;
				infile = (*(j+1));
				// 备份标准输入
				f_stdin = dup2(0, 7);
				// 打开输入文件
				int fin = open(path_in, O_RDONLY);
				if (fin == -1)
				{
					printf("%s: No such file or directory\n", path_in);
					return;
				}
				// 用输入文件替代标准输入
				dup2(fin, 0);
				close(fin);
			}
			// 扫描到重定向输出符号，将输出从stdout修改到文件
			if (*j == ">")
			{
				const char* path_out = (*(j+1)).c_str();
				// 备份标准输出
				f_stdout = dup2(1, 8);
				// 打开输出文件
				int fout = open(path_out, O_WRONLY|O_CREAT|O_TRUNC);
				if (fout == -1)
				{
					printf("%s: invalid file or directory\n", path_out);
					return;
				}
				// 标记：发生输出重定向，并备份输出文件名
				reout = true;
				outfile = (*(j+1));
				// 用输出文件替换标准输出
				dup2(fout, 1);
				close(fout);
			}
			// 扫描到重定向输出符号，将输出从stdout修改到文件，且写方式为追加到文件末
			if (*j == ">>")
			{
				const char* path_out = (*(j+1)).c_str();
				// 备份标准输出
				f_stdout = dup2(1, 8);
				// 打开输出文件
				int fout = open(path_out, O_WRONLY|O_CREAT|O_APPEND);
				if (fout == -1)
				{
					printf("%s: No such file or directory\n", path_out);
					return;
				}
				// 标记：发生输出重定向，并备份输出文件名
				reappend = true;
				outfile = (*(j+1));
				// 用输出文件替换标准输出
				dup2(fout, 1);
				close(fout);
			}
			// 扫描到后台执行符号，将标志是否后台执行的变量bg设为true，并从参数中去掉&符号
			if (*j == "&")
			{
				bg = true;
				c_word.erase(j);
				j--;
			}
		}

		// 发生输入重定向时，从输入文件中读取内容，存放在redir_word中
		if (in_redir)
		{
			redir_word.clear();
			redir_word.push_back(c_word[0]);
			char word_buf[MAX_WORD_LEN];
			while (fscanf(file_in, "%s", word_buf) != EOF)
			{
				string word = word_buf;
				redir_word.push_back(word);
			}
			fclose(file_in);
		}

		// 去掉命令行参数中的重定向部分
		for (vector<string>::iterator j = c_word.begin(); j != c_word.end(); j++)
		{
			if (*j == "<" || *j == ">" || *j == ">>")
			{
				c_word.erase(j);
				c_word.erase(j);
				j--;
				if (j == c_word.end()) break;
			}
		}
		
		// 根据命令确定command_state
		// 若命令在c_map中则使用c_map中对应的STATE
		// 否则令command_state为STATE_COMMAND
		map<string, Command_state>::iterator c_itr = c_map.find(c_word[0]);
		if (c_itr == c_map.end())
		{
			command_state = STATE_COMMAND;
		}
		else
			command_state = c_map[c_word[0]];

		// 非后台执行命令，直接调用c_exec执行
		if (!bg)
		{
			c_exec(c_word);
		}
		// 后台执行命令
		else
		{
			// 建立子进程，并令子进程独立出一个进程组，与myshell脱钩
			pid_t pid = fork();
			if (pid == 0)
			{
				setpgid(0, 0);
				c_exec(c_word);
			}
			else
			{
				setpgid(pid, pid);
				// 在后台作业链表中加入当前作业
				job* j = new job;
				j->pid = pid;
				j->pgid = getpgid(pid);
				j->job_argc = c_word.size();
				for (string k : c_word)
				{
					j->job_argv.push_back(k);
				}
				j_list->add_job(j);
				j->print_job();

				// 不等待子进程状态发生变化，立即返回
				waitpid(pid, 0, WNOHANG);
			}
		}

		// 同步管道文件
		sync_pipe();

		// 检查是否有后台作业已经完成
		j_list->job_check();
	}
}

// 函数：c_exec
// 功能：执行指令
// 输入：命令行参数列表c_word
// 返回：无
void c_exec(vector<string> c_word)
{
	// 发生if语法错误，报错并停止执行命令
	if (command_state != STATE_THEN && (if_state == IF_TRUE || if_state == IF_FALSE))
	{
		printf("-myshell: syntax error\n");
		if_state = NO_IF;
		return;
	}
	// if条件语句值为false，不执行语句
	if (if_state == THEN_FALSE && command_state != STATE_FI)
	{
		return;
	}

	// 根据当前状态选择执行命令
	switch (command_state)
	{
		// 命令：pwd
		// 功能：获取并显示当前工作目录
		case STATE_PWD:
		{
			getcwd(working_directory, MAX_PATH_LEN);
			printf("%s\n", working_directory);
			break;
		}
		// 不在c_map中的命令
		// 首先尝试作为变量赋值语句解析
		// 若不能则作为外部命令执行
		case STATE_COMMAND:
		{
			// 作为变量赋值语句执行
			if (c_word.size() > 1 && c_word[1] == "=")
			{
				// 优先作为环境变量解析
				if (getenv(c_word[0].c_str()))
				{
					setenv(c_word[0].c_str(), c_word[2].c_str(), 1);
					break;
				}

				// 插入变量名到变量的映射的键值对
				var_map.insert(std::pair<string, int>(c_word[0], var_num++));
				// 变量赋值
				if (c_word.size() > 2)
				{
					var_list.push_back(c_word[2]);
				}
				else
				{
					var_list.push_back("");
				}
				
				break;
			}

			// 作为外部命令执行，设置参数列表
			char* command = (char*)c_word[0].c_str();
			char* argv[c_word.size() + 1] = {0};
			for (int i = 0; i < c_word.size(); i++)
			{
				argv[i] = (char*)c_word[i].c_str();
			}
			argv[c_word.size()] = NULL;

			// 前台执行
			if (!bg)
			{
				pid_t pid = fork();
				
				if (pid == 0)
				{
					execvp(argv[0], argv);
					printf("%s: command not found\n", argv[0]);
					exit(127);	
				}
				else
				{
					waitpid(pid, 0, 0);
				}
			}
			// 后台执行
			// 由于后台执行命令在c_interpret中已经完成了fork工作，
			// 此处无需在fork出一个新进程
			else
			{
				execvp(argv[0], argv);
				printf("%s: command not found\n", argv[0]);
				exit(127);
			}
			
			break;
		}
		// 命令：clr, clear
		// 功能：清屏
		case STATE_CLEARSCREEN:
		{
			printf("\x1b[H\x1b[2J") ;
			break;
		}
		// 命令：exit, quit
		// 功能：退出myshell	
		case STATE_HALT:
		{
			halt = true;
			break;
		}
		// 命令：cd
		// 功能：修改当前工作路径
		case STATE_CD:
		{
			// 无参数时默认修改为主目录，否则修改为参数1中包含的路径
			string path = (c_word.size() == 1) ? getenv("HOME") : format_path(c_word[1]);
			if (chdir(path.c_str()) != 0)
			{
				printf("%s: No such file or directory\n", path.c_str());
			}
			break;
		}
		// 命令：environ
		// 功能：显示环境变量
		case STATE_ENV:
		{
			envir = environ;
			while (*envir)
			{
				printf("%s\n", *envir++);
			}
			break;
		}
		// 命令：echo
		// 功能：输出内容
		case STATE_ECHO:
		{
			c_word.erase(c_word.begin());
			for (vector<string>::iterator i = c_word.begin(); i != c_word.end(); i++)
			{
				if (i != c_word.begin())
					printf(" ");
				printf("%s", (*i).c_str());
			}
			printf("\n");
			break;
		}
		// 命令：dir
		// 功能：显示目录中的文件和子目录
		case STATE_DIR:
		{
			// 无参数时默认显示当前目录内容
			string path = (c_word.size() == 1) ? getcwd(NULL, 0) : format_path(c_word[1]);
			const char* temp_path = path.c_str();

			// 打开目录
			DIR* p_dir = opendir(temp_path);
			if (!p_dir) 
			{
				printf("%s: No such file or directory\n", temp_path);
				break;
			}
			struct dirent* dir_item;
			// 遍历目录文件
			while (dir_item = readdir(p_dir))
			{	
				if (dir_item->d_name[0] != '.') 
				{
					if (strlen(dir_item->d_name) < 15)
					{
						printf("%-15s", dir_item->d_name);
					}
					else if (strlen(dir_item->d_name) < 30)
					{
						printf("%-30s", dir_item->d_name);
					}
					else {
						printf("%s\n", dir_item->d_name);
					}
				}
			}
			printf("\n");
			closedir(p_dir);
			break;
		}
		// 命令：time
		// 功能：显示当前时间
		case STATE_TIME:
		{
			time_t raw;
			time(&raw);
			// fprintf(file_out, "%s", ctime(&raw));
			printf("%s", ctime(&raw));
			break;
		}
		// 命令：exec
		// 功能：使用目标命令替代当前进程
		case STATE_EXEC:
		{	
			if (c_word.size() <= 1)
				exit(0);
			// 获得待执行的命令及其命令行参数
			const char* command = (char*)c_word[1].c_str();
			char* argv[c_word.size()];
			for (int i = 1; i < c_word.size(); i++)
			{
				argv[i - 1] = (char*)c_word[i].c_str();
			}
			argv[c_word.size() - 1] = NULL;
			// 调用execvp执行命令
			if (!execvp(argv[0], argv))
			{
				printf("%s: command not found\n", argv[0]);
			}
			break;
		}
		// 命令：umask
		// 功能：获得或修改文件创建时权限掩码
		case STATE_UMASK:
		{
			// 若umask无参数，则获得并输出当前umask
			if (c_word.size() <= 1)
			{
				int present_umask = umask(0);
				umask(present_umask);
				printf("%04o\n", present_umask);
			}
			// umask有参数，则修改当前umask
			else 
			{
				try
				{
					mode_t cmask_oct = stoi(c_word[1]);
					mode_t cmask_dec = 0, pow = 1;
					while (cmask_oct > 0)
					{
						if (cmask_oct % 10 >= 8)
						{
							cout << "umask: \'" << c_word[1] << "\': invalid symbolic mode operator" << endl;
							return;
						}
						cmask_dec += cmask_oct % 10 * pow ;
						cmask_oct /= 10;
						pow *= 8;
					}

					umask(cmask_dec);
				}
				catch(const std::invalid_argument& e)
				{
					cout << "umask: \'" << c_word[1] << "\': invalid symbolic mode operator" << endl;
				}
				
			}
			break;
		}
		// 命令：fg
		// 功能：将后台进程组转至前台
		case STATE_FG:
		{
			// 从后台作业链表中选取一个作业j
			job* j = NULL;
			// 无参数时，默认选取1号作业
			if (c_word.size() == 1)
			{
				j = j_list->head;
				if (!j)
				{
					printf("fg: current: no such job\n");
					return;
				}
			}
			// 有参数时
			else 
			{
				// 若参数以%开头，先去掉%
				if (c_word[1].substr(0, 1) == "%")
				{
					c_word[1].erase(0, 1);
				}
				
				string jobid_str = c_word[1];
				// 若jobid为数字，则作为作业号解析
				try
				{
					int jobid = std::stoi(jobid_str);
					job* k;
					// 寻找作业号为jobid的作业
					for (k = j_list->head; k; k = k->next)
					{
						if (k->jobid == jobid)
						{
							j = k;
							break;
						}
					}
					// 没有找到，报错返回
					if (!k)
					{
						printf("fg: %d: no such job\n", jobid);
						return;
					}
				}
				// 若jobid不是数字，尝试以+-两个符号或作业命令行参数argv[0]进行解析
				catch(const std::invalid_argument& e)
				{
					if (jobid_str == "+")
						j = j_list->head;
					else if (jobid_str == "-")
						j = j_list->head->next;
					else 
					{
						for (job* k = j_list->head; k; k = k->next)
						{
							if (jobid_str == k->job_argv[0])
							{
								j = k;
								break;
							}
						}
					}
				}
					
			}
			if (!j)
			{
				printf("fg: No such job\n");
				return;
			}

			// 获得j和shell的进程组id
			pid_t pgid= j->pgid;
			pid_t shell_pid = getpid();

			// 修改SIGTTOU信号的处理方式，避免shell在后台向stdout输出造成错误
			signal(SIGTTOU, SIG_IGN);

			// 将程序前台控制权交给j所在的进程组
			tcsetpgrp(STDIN_FILENO, pgid);
			if (kill(j->pid, SIGCONT))
			{
				printf("fg: No such job\n");
				return;
			}
			// 等待进程组结束
			waitpid(-pgid, NULL, 0);
			
			// 恢复shell所在进程组的前台控制权，并恢复SIGTTOU信号的处理方式
			tcsetpgrp(STDIN_FILENO, getpgid(shell_pid));
			signal(SIGTTOU, SIG_DFL);

			break;
		}
		// 命令：bg
		// 功能：使后台暂停运行的进程继续运行
		case STATE_BG:
		{
			// 从后台作业链表中选取一个作业j
			job* j;
			// 无参数时，默认选取1号作业
			if (c_word.size() == 1)
			{
				j = j_list->head;
				if (!j)
				{
					printf("fg: current: no such job\n");
					return;
				}
			}
			// 有参数时
			else 
			{
				// 若参数以%开头，先去掉%
				if (c_word[1].substr(0, 1) == "%")
				{
					c_word[1].erase(0, 1);
				}
				
				string jobid_str = c_word[1];
				// 若jobid为数字，则作为作业号解析
				try
				{
					int jobid = std::stoi(jobid_str);
					job* k;
					// 寻找作业号为jobid的作业
					for (k = j_list->head; k; k = k->next)
					{
						if (jobid == k->jobid)
						{
							j = k;
							break;
						}
					}
					// 没有找到，报错返回
					if (!k)
					{
						printf("fg: %d: no such job\n", jobid);
						return;
					}
				}
				// 若jobid不是数字，尝试以+-两个符号或作业命令行参数argv[0]进行解析
				catch(const std::invalid_argument& e)
				{
					if (jobid_str == "+")
						j = j_list->head;
					else if (jobid_str == "-")
						j = j_list->head->next;
					else 
					{
						for (job* k = j_list->head; k; k = k->next)
						{
							if (k->job_argv[0] == jobid_str)
							{
								j = k;
								break;
							}
						}
					}
				}
					
			}
			if (!j)
			{
				printf("bg: No such job\n");
				return;
			}

			// 向j发送SIGCONT信号使其继续运行
			kill(j->pid, SIGCONT);
			break;
		}
		// 命令：jobs
		// 功能：列表显示后台作业
		case STATE_JOBS:
		{
			// 调用job_list类中的jobs()方法
			j_list->jobs();
			break;
		}
		// 命令：shift
		// 功能：移动命令行参数位置
		case STATE_SHIFT:
		{
			int shift_bit = 0;
			// 无参数时默认移动一位
			if (c_word.size() == 1)
			{
				shift_bit = 1;
			} 
			else if (stoi(c_word[1]) < 9)
			{
				shift_bit = stoi(c_word[1]);
			}
			else
			{
				shift_bit = 9;
			}
			
			// 向左移动命令行参数
			for (int i = 1; i <= 9 - shift_bit; i++)
			{
				parameter[i] = parameter[i + shift_bit];
			}
			for (int i = 10 - shift_bit ; i < 10; i++)
			{
				parameter[i] = NULL;
			}
			break;
		}
		// 命令：set
		// 功能：修改命令行参数的值
		case STATE_SET:
		{
			int para_num = c_word.size() - 1;
			for (int i = 1; i <= para_num && i < 10; i++)
			{
				parameter[i] = c_word[i].c_str();
			}
			for (int i = para_num + 1; i < 10; i++)
			{
				parameter[i] = NULL;
			}
			break;
		}
		// 命令：if
		// 功能：执行if语句
		case STATE_IF:
		{
			// 若进入if语句前不是NO_IF状态，说明出现了语法错误
			if (if_state != NO_IF)
			{
				printf("-mysql: syntax error\n");
				if_state = NO_IF;
				return;	
			}
			// 从命令行参数中去掉if
			c_word.erase(c_word.begin());
			// 调用judge_expression函数判断表达式真值
			bool ans = judge_expression(c_word);
			// 根据真值确定下一步状态
			if (ans)
			{
				if_state = IF_TRUE;
			}
			else
			{
				if_state = IF_FALSE;
			}
			break;
		}
		// 在if语句中读到then
		// 选择是否执行语句
		case STATE_THEN:
		{
			if (if_state == IF_TRUE)
			{
				if_state = THEN_TRUE;
			}
			else if (if_state == IF_FALSE)
			{
				if_state = THEN_FALSE;
			}
			else 
			{
				printf("-myshell: syntax error\n");
				if_state = NO_IF;	
			}
			break;
		}
		// 命令：fi
		// 功能：if语句结束，根据表达式真值决定是否执行if内语句
		case STATE_FI:
		{
			// 恢复if_state
			if_state = NO_IF;

			// 依次解析执行if_buffer内的语句
			for (string i : if_buffer)
			{
				c_interpret(i);
			}
			// 清空if_buffer
			while (if_buffer.begin() != if_buffer.end())
			{
				if_buffer.erase(if_buffer.begin());
			}
			break;
		}
		// 命令：unset
		// 功能：将变量置空
		case STATE_UNSET:
		{
			try
			{
				int varid = stoi(c_word[1]);
				if (varid > 0 && varid < 10)
				{
					parameter[varid] = NULL;
				}
			}
			catch(const std::invalid_argument& e) { }
			
			if (var_map.find(c_word[1]) != var_map.end())
			{
				var_list[var_map[c_word[1]]] = "";
			} 

			break;
		}
		// 命令：help
		// 功能：调用man命令显示帮助并使用more过滤
		case STATE_HELP:
		{
			// -----构造调用man的命令语句-----
			string help = "man ";
			help = help + c_word[1] + " | more ";
			
			for (vector<string>::iterator i = c_word.begin() + 2; i < c_word.end(); i++)
			{
				help += *i;
				help += " ";
			}
			
			if (reout)
			{
				help += "> ";
				help += outfile;
				help += " ";
			}
			if (reappend)
			{
				help += ">> ";
				help += outfile;
				help += " ";
			}

			// 恢复标准输入输出，准备重新解析命令
			if (f_stdout != -1)
				dup2(f_stdout, 1);
			if (f_stdin != -1)
				dup2(f_stdin, 0);

			rein = false;
			reout = false;
			reappend = false;

			// 调用语句解析器解析执行命令
			c_interpret(help);

			break;
		}
	}
	// 恢复标准输入输出
	if (f_stdout != -1)
		dup2(f_stdout, 1);
	if (f_stdin != -1)
		dup2(f_stdin, 0);
	rein = false;
	reout = false;
	reappend = false;
}

// ----------------后台任务相关函数----------------

// 函数：print_job
// 功能：打印单个job的jobid和对应进程pid，在新建后台作业时使用
// 输入：无
// 返回：无
void job::print_job()
{
	printf("[%d] %d\n", jobid, pid);
}

// 函数：add_job
// 功能：将一个job挂到后台作业链表上
// 输入：待加入链表的job指针
// 返回：无
void job_list::add_job(job* j)
{
	if (!j)
		return;
	
	// 若当前链表为空，则链表的首尾均为j；否则将j放到链表的最后，并更新相关指针
	if (!head)
	{
		head = j;
		tail = j;
		j->jobid = 1;
	}
	else
	{
		j->last = tail;
		tail = j;
		j->last->next = j;
		j->jobid = (j->last->jobid) + 1;
	}
}

// 函数: print_full_job
// 功能：完整显示一个后台作业的信息
// 输入：无
// 返回：无
void job::print_full_job()
{
	printf("[%d] %d", jobid, pid);
	if (jobid == 1) printf("+");
	else if (jobid == 2) printf("-");
	printf("\t");
	switch(state)
	{
		case JOB_RUNNING:
			printf("Running\t\t");
			break;
		case JOB_STOPPED:
			printf("Stopped\t\t");
			break;
		case JOB_TERMINATED:
			printf("Terminated\t");
			break;
		case JOB_DONE:
			printf("Done\t");
			break;
	}
	for (string k : this->job_argv)
	{
		cout << k << " ";
	}
	printf("\n");
}

// 函数：del_job
// 功能：将一个job挂到后套作业链表上
// 输入：待加入链表的job指针
// 返回：无
void job_list::del_job(job* j)
{
	if (head == j)
		head = j->next;
	if (tail == j)
		tail = j->last;
	if (j->last)
		j->last->next = j->next;
	if (j->next)
		j->next->last = j->last;
	
	job* ptr = j->next;
	while (ptr)
	{
		ptr->jobid--;
		ptr = ptr->next;
	}
		
	delete j;
}

// 函数：job_check
// 功能：检查并更新一个后台作业的状态
// 		若已完成或已终止则从后台作业表中去除
// 输入：无
// 返回：无
int job_list::job_check()
{
	job* j = head;
	// 遍历检查作业链表中的作业情况
	while (j)
	{
		// 作业状态是否发生改变
		int exit_status = waitpid(j->pid, &(j->status), WNOHANG | WUNTRACED);
		// 作业是否暂停
		int stop_status =  WIFSTOPPED(j->status);
		// 作业是否被终止
		int term_status = WIFSIGNALED(j->status);

		// 作业已完成
		if (exit_status == -1 || (exit_status > 0 && stop_status == 0))
		{
			j->state = job::JOB_DONE;
		}
		// 作业被终止
		else if (exit_status > 0 && term_status)
		{
			j->state = job::JOB_TERMINATED;
		}
		// 作业被暂停
		else if (exit_status > 0 && stop_status)
		{
			j->state = job::JOB_STOPPED;
		}
		// 作业正在运行
		else
		{
			j->state = job::JOB_RUNNING;
		}
		if (j->state == job::JOB_DONE || j->state == job::JOB_TERMINATED)
		{
			j->print_full_job();
		}
		job* next = j->next;
		
		j = next;
	}
	j = head;
	// 删除已经完成或已被终止的作业
	while (j)
	{
		if (j->state == job::JOB_DONE || j->state == job::JOB_TERMINATED)
		{
			del_job(j);
		}
		job* next = j->next;
		j = next;
	}	
}


// 函数：jobs
// 功能：列表显示并更新后台作业状态
// 输入：无
// 返回：无
int job_list::jobs()
{
	// 该函数主要内容与job_check原理相同，只是加了一个打印后台信息的过程
	// 功能注释请参考job_check
	job* j = head;
	while (j)
	{
		int exit_status = waitpid(j->pid, &(j->status), WNOHANG | WUNTRACED);
		int stop_status =  WIFSTOPPED(j->status);
		int term_status = WIFSIGNALED(j->status);
		if (exit_status == -1 || (exit_status > 0 && stop_status == 0))
		{
			j->state = job::JOB_DONE;
		}
		else if (exit_status > 0 && term_status)
		{
			j->state = job::JOB_TERMINATED;
		}
		else if (exit_status > 0 && stop_status)
		{
			j->state = job::JOB_STOPPED;
		}
		else
		{
			j->state = job::JOB_RUNNING;
		}
		// 打印job信息
		j->print_full_job();
		job* next = j->next;
		
		j = next;
	}
	j = head;
	while (j)
	{
		if (j->state == job::JOB_DONE || j->state == job::JOB_TERMINATED)
		{
			del_job(j);
		}
		job* next = j->next;
		j = next;
	}	
}


