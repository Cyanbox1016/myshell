// 文件名：command.h
// 作者：3180105481 王泳淇
// 功能：包括myshell实现依赖的部分函数声明和类定义
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

// 类：job
// 功能：存储一个后台作业的信息；后台作业链表的组成元素
class job
{
public:
	// 作业信息：进程号、进程组号、作业号以及命令行参数个数、命令行参数
	pid_t pid;
	pid_t pgid;
	pid_t jobid;
	int job_argc;
	std::vector<std::string> job_argv;

	// status：存储waitpid返回的进程状态值
	int status;
	// state：进程状态，分运行、暂停、终止和完成四种状态
	enum state_id
	{
		JOB_RUNNING,
		JOB_STOPPED,
		JOB_TERMINATED,
		JOB_DONE
	} state;

	// 指针，指向后台作业链表中的上一作业和下一作业
	job* last;
	job* next;

	// 默认构造函数
	job()
	{
		pid = -1;
		pgid = -1;
		jobid = -1;
		job_argc = 0;
		last = NULL;
		next = NULL;
	}

	// 析构函数
	~job() {}

	// 类方法：print_job
	// 功能：打印作业的作业号和进程号
	void print_job();

	// 类方法：print_full_job
	// 功能：打印作业的完整信息，调用jobs命令时使用
	void print_full_job();
};

// 类：job_list
// 功能：后台作业链表，用于存储和管理后台作业
class job_list
{
public:
	// 成员：head和tail指向链表的头节点和尾节点
	static job *head;
	static job *tail;

	// 析构函数
	~job_list() {}

	// 类方法：add_job
	// 功能：向链表中添加后台作业
	void add_job(job*);

	// 类方法：del_job
	// 功能：从链表中删除后台作业
	void del_job(job*);

	// 类方法：job_check
	// 功能：检查后台作业状态是否有变化，若完成则改变其状态、在屏幕上显示并从后台作业表中删除
	int job_check();

	// 类方法：jobs
	// 功能：列表显示后台作业状态
	int jobs();
};

// Command_state：标志命令执行状态的枚举类型
// c_exec根据command_state选取执行命令的方式
// 枚举的元素以命令类型命名
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
	STATE_FG,
	STATE_JOBS,
	STATE_SHIFT,
	STATE_SET,
	STATE_BG,
	STATE_IF,
	STATE_THEN,
	STATE_FI,
	STATE_UNSET,
	STATE_HELP
} command_state;

// If_state：标志if语句进行到的位置和状态
// 在if语句外默认为NO_IF状态
// 读到if后根据表达式的真值有IF_TRUE和IF_FALSE两种情况
// 读到THEN后又有THEN_TRUE和THEN_FALSE两种情况
// 根据if_state判断语句执行的方式
extern enum If_state
{
	NO_IF,
	IF_TRUE,
	IF_FALSE,
	THEN_TRUE,
	THEN_FALSE
} if_state;

// Test_expr: 标志test所判断的表达式的类型
// 根据表达式类型返回相应的值
extern enum Test_expr
{
	STR_NE,
	STR_EQ,
	STR_LE,
	STR_GR,
	STR_NONZERO,
	STR_ZERO,
	DIR_F,
	FILE_F,
	FILE_R,
	FILE_W,
	FILE_E,
	INT_EQ,
	INT_GE,
	INT_GT,
	INT_LE,
	INT_LT,
	INT_NE
} test_expr;

// ----------------外部变量表----------------

// c_map：用于将输入的命令映射到相应的command_state
extern std::map<std::string, Command_state> c_map;
// var_map：用于将变量名映射到变量列表下标
extern std::map<std::string, int> var_map;
// var_num变量表示用户定义的变量数目
extern int var_num;
// file_in, file_out：指向输入输出文件的FILE*指针   
extern FILE *file_in, *file_out;
// shell_path：myshell的路径
// shell_dir：myshell所在的目录
extern char* shell_path, *shell_dir;
// parameter：命令行参数表
extern const char* parameter[10];
// f_stdin, f_stdout：用于备份标准输入输出的文件号
extern int f_stdin, f_stdout;
// j_list：后台作业链表，为双向链表
extern job_list *j_list;
// if_buffer：用于存放if语句内部的命令
extern std::vector<std::string> if_buffer;
// var_list：存放变量的vector
extern std::vector<std::string> var_list;
// halt：标志是否停机，当halt为false时myshell持续运行
extern bool halt;
// working_directory：当前工作目录
extern char working_directory[MAX_PATH_LEN];

// ----------------函数声明----------------

// 函数：init_shell()
// 功能：进行一些shell初始化工作，主要包括获取、设置环境变量和建立后台作业链表
// 输入：无
// 返回：无
void init_shell();

// 函数：init_map
// 功能：初始化c_map，将指令类型和执行指令的状态的键值对插入到c_map中
// 输入：无
// 返回：无
void init_map();

// 函数：init_test
// 功能：初始化test_map，将符号和表达式类型的键值对插入到test_map中
// 输入：无
// 返回：无
void init_test();

// 函数：split_line
// 功能：将输入的字符串用指定的字符分割成多个子字符串
// 输入：存放子字符串的vector word，待分割字符串line，可指定的分割字符列表splitter，默认值为空白字符列表
// 返回：无
void split_line(std::vector<std::string>& words, std::string line, std::string splitter);

// 函数：c_interpret
// 功能：解析输入的命令行，判定命令类型，并将处理结果进一步
// 		交给c_exec执行
// 输入：命令行字符串c_line
// 返回：无
void c_interpret(std::string);

// 函数：c_exec
// 功能：执行指令
// 输入：命令行参数列表c_word
// 返回：无
void c_exec(std::vector<std::string> c_word);

// 函数：clear_pipe
// 功能：清除管道文件内容，去除上次使用管道遗留下来的内容
// 输入：无
// 返回：无
void clear_pipe();

