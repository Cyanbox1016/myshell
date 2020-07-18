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

bool halt = false;
char working_directory[MAX_PATH_LEN];
char temp_path[MAX_PATH_LEN];
enum Command_state command_state;
map<string, Command_state> c_map;
FILE *file_in, *file_out;

extern char** environ;
char** envir;

char* shell_path, *shell_dir;
char* pipe_in_path, *pipe_out_path;

int f_stdin, f_stdout;

void init_shell()
{
	shell_path = (char*)malloc(MAX_PATH_LEN);
	shell_dir = (char*)malloc(MAX_PATH_LEN);

	readlink("/proc/self/exe", shell_path, MAX_PATH_LEN - 1);
	setenv("SHELL", shell_path, 1);

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
	
	pipe_in_path = (char*)malloc(MAX_PATH_LEN);
	pipe_out_path = (char*)malloc(MAX_PATH_LEN);
	strcpy(pipe_in_path, shell_dir);
	strcpy(pipe_out_path, shell_dir);
	strcat(pipe_in_path, "pipe_in");
	strcat(pipe_out_path, "pipe_out");

	string env_path = getenv("PATH");
	env_path += ":";
	env_path += shell_dir;
	setenv("PATH", env_path.c_str(), 1);
}

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
}

string trim_string (const string& str)
{
	string s = str;
	
	string blanks("\f\v\r\t\n ");
	s.erase(0, s.find_first_not_of(blanks));
	s.erase(s.find_last_not_of(blanks) + 1);
	return s;
}

string trim_stream(const stringstream& ss) 
{
	string s = ss.str();
	return trim_string(s);
}

inline void split_line(vector<string>& words, string line)
{
	size_t pos;
	
	words.clear();
	while ((pos = line.find_first_of("\f\v\r\t\n ")) != line.npos)
	{
		words.push_back(line.substr(0, line.find_first_of("\f\v\r\t\n ")));
		line.erase(0, line.find_first_of("\f\v\r\t\n "));
		line = trim_string(line);
	}
	words.push_back(line);
}

inline string format_path(const string& path)
{
	string ans = path;
	while (ans.find("~") != ans.npos) 
	{
		ans.replace(ans.find("~"), 1, getenv("HOME"));
	}
	while (ans.find("$HOME") != ans.npos)
	{
		ans.replace(ans.find("$HOME"), 5, getenv("HOME"));
	}

	return ans;
}

void clear_pipe()
{
	fclose(fopen(pipe_out_path, "w"));

	fclose(fopen(pipe_in_path, "w"));
}

inline int sync_pipe()
{
	fclose(fopen(pipe_in_path, "w"));

	int pipe_in, pipe_out;
	char buf[1024];
	size_t nread;

	pipe_in = open(pipe_in_path, O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
	pipe_out = open(pipe_out_path, O_RDONLY);
	if (pipe_in < 0 || pipe_out < 0)
		return -1;
	
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

	fclose(fopen(pipe_out_path, "w"));

	return 0;
}

void c_interpret(string c_line)
{
	// string c_line;
	// getline(cin, c_line);

	if (cin.eof())
	{
		printf("exit\n");
		exit(0);
	}

	if (c_line == "")
	{
		exit(0);
	}

	size_t pos = 0;
	vector<string> c_command;
	vector<string> c_word;
	while ((pos = c_line.find("|")) != c_line.npos)
	{	
		c_command.push_back(trim_string(c_line.substr(0, (int)pos)));
		c_line.erase(0, pos + 1);
	}
	c_command.push_back(trim_string(c_line));

	// newly added
	for (vector<string>::iterator i = c_command.begin(); i != c_command.end(); i++)
	{
		bool in_redir = false;
		vector<string> redir_word;
 
		split_line(c_word, *i);
		
		file_in = stdin;
		file_out = stdout;
		if (c_command.size() == 1) 
		{
			file_in = stdin;
			file_out = stdout;
			in_redir = false;
		}
		else if (i == c_command.begin())
		{
			file_in = stdin;
			file_out = fopen(pipe_out_path, "w");
			if (!file_out)
			{
				printf("ERROR: pipe buffer not available");
				return;
			}
			in_redir = false;
		}
		else if (i == c_command.end() - 1)
		{
			file_in = fopen(pipe_in_path, "r");
			if (!file_in)
			{
				printf("ERROR: pipe buffer not available");
				return;
			}
			file_out = stdout;
			in_redir = true;
		}
		else
		{
			file_in = fopen(pipe_in_path, "r");
			if (!file_in)
			{
				printf("ERROR: pipe buffer not available");
				return;
			}
			file_out=fopen(pipe_out_path, "w");
			if (!file_out)
			{
				printf("ERROR: pipe buffer not available");
				return;
			}
			in_redir = true;
		}

		for (vector<string>::iterator j = c_word.begin(); j != c_word.end(); j++)
		{
			if (*j == "<")
			{
				const char* path_in = (*(j+1)).c_str();
				file_in = fopen(path_in, "r");
				if (!file_in)
				{
					printf("%s: invalid file or directory\n", path_in);
					return;
				}
				
				in_redir = true;

				f_stdin = dup(0);
				int fin = open(path_in, O_RDONLY);
				if (fin == -1)
				{
					printf("%s: No such file or directory\n", path_in);
					return;
				}
				dup2(fin, 0);
				close(fin);

			}
			if (*j == ">")
			{
				const char* path_out = (*(j+1)).c_str();
				f_stdout = dup(1);
				int fout = open(path_out, O_WRONLY|O_CREAT|O_TRUNC);
				if (fout == -1)
				{
					printf("%s: No such file or directory\n", path_out);
					return;
				}
				dup2(fout, 1);
				close(fout);
			}
			if (*j == ">>")
			{
				const char* path_out = (*(j+1)).c_str();
				f_stdout = dup(1);
				int fout = open(path_out, O_WRONLY|O_CREAT|O_APPEND);
				if (fout == -1)
				{
					printf("%s: No such file or directory\n", path_out);
					return;
				}
				dup2(fout, 1);
				close(fout);
			}
		}

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

		for (vector<string>::iterator j = c_word.begin(); j != c_word.end(); j++)
		{
			if (*j == "<" || *j == ">" || *j == ">>")
			{
				j = c_word.erase(j);
				j = c_word.erase(j);
				if (j == c_word.end()) break;
			}
		}

		map<string, Command_state>::iterator c_itr = c_map.find(c_word[0]);
		if (c_itr == c_map.end())
		{
			command_state = STATE_COMMAND;
		}
		else
			command_state = c_map[c_word[0]];
		
		c_exec(c_word);

		sync_pipe();
	}
}

void c_exec(vector<string> c_word)
{
	switch (command_state)
	{
		case STATE_PWD:
		{
			getcwd(working_directory, MAX_PATH_LEN);
			// fprintf(file_out, "%s\n", working_directory);
			printf("%s\n", working_directory);
			break;
		}
		case STATE_COMMAND:
		{
			pid_t pid = fork();
			if (pid == 0)
			{
				char* command = (char*)c_word[0].c_str();
				char* argv[c_word.size() + 1] = {0};
				for (int i = 0; i < c_word.size(); i++)
				{
					argv[i] = (char*)c_word[i].c_str();
				}
				argv[c_word.size()] = NULL;
				execvp(argv[0], argv);
				printf("%s: command not found\n", argv[0]);
				exit(127);
			}
			else
			{
				waitpid(pid, 0, 0);
			}
			break;
		}
		case STATE_CLEARSCREEN:
		{
			printf("\x1b[H\x1b[2J") ;
			break;
		}	
		case STATE_HALT:
		{
			halt = true;
			break;
		}
		case STATE_CD:
		{
			string path = (c_word.size() == 1) ? getenv("HOME") : format_path(c_word[1]);
			if (chdir(path.c_str()) != 0)
			{
				printf("%s: No such file or directory\n", path.c_str());
			}
			break;
		}
		case STATE_ENV:
		{
			envir = environ;
			while (*envir)
			{
				// fprintf(file_out, "%s\n", *envir++);
				printf("%s\n", *envir++);
			}
			break;
		}
		case STATE_ECHO:
		{
			c_word.erase(c_word.begin());
			for (vector<string>::iterator i = c_word.begin(); i != c_word.end(); i++)
			{
				if (i != c_word.begin())
					// fprintf(file_out, " ");
					printf(" ");
				// fprintf(file_out, "%s", (*i).c_str());
				printf("%s", (*i).c_str());
			}
			printf("\n");
			break;
		}
		case STATE_DIR:
		{
			string path = (c_word.size() == 1) ? getcwd(NULL, 0) : format_path(c_word[1]);
			const char* temp_path = path.c_str();
			// cout << "yes" << endl;
			DIR* p_dir = opendir(temp_path);
			if (!p_dir) 
			{
				printf("%s: No such file or directory\n", temp_path);
				break;
			}
			struct dirent* dir_item;
			while (dir_item = readdir(p_dir))
			{	
				if (dir_item->d_name[0] != '.') 
				{
					if (strlen(dir_item->d_name) < 15)
					{
						printf("%-15s", dir_item->d_name);
					}
					else if (strlen(dir_item->d_name) < 25)
					{
						printf("%-25s", dir_item->d_name);
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
		case STATE_TIME:
		{
			time_t raw;
			time(&raw);
			// fprintf(file_out, "%s", ctime(&raw));
			printf("%s", ctime(&raw));
			break;
		}
		case STATE_EXEC:
		{	
			if (c_word.size() <= 1)
				exit(0);
			const char* command = (char*)c_word[1].c_str();
			char* argv[c_word.size()];
			for (int i = 1; i < c_word.size(); i++)
			{
				argv[i - 1] = (char*)c_word[i].c_str();
			}
			argv[c_word.size() - 1] = NULL;
			execvp(argv[0], argv);
			break;
		}
		case STATE_UMASK:
		{
			if (c_word.size() <= 1)
			{
				mode_t present_umask = umask(0);
				umask(present_umask);
				// fprintf(file_out, "%u\n", present_umask);
				printf("%u\n", present_umask);
			}
			else 
			{
				mode_t new_umask;
				try
				{
					new_umask = std::stoi(c_word[1]);
					umask(new_umask);
				}
				catch(const std::invalid_argument& e)
				{
					cout << "umask: \'" << new_umask << "\': invalid symbolic mode operator" << endl;
				}
				
			}
			break;
		}
	}
	dup2(f_stdout, 1);
	dup2(f_stdin, 0);
}
