#ifndef SFISH_H
#define SFISH_H
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <ucontext.h>
#include <time.h>
extern char* previous_dir;
extern char* current_dir;
extern char* prompt;
extern char *argv[100];
extern char *local_envp[100];
extern int errno;
extern volatile sig_atomic_t alrm_set;

int cmd_cd(char* word);
int process_cmd(char* cmd,int pwd_size);
int parse_cmd(char* line, int pwd_size, char** argv);
void usage(void);
void set_prompt(int prompt_space, char* prompt);
char* get_cwd(char* cwd_buf,char* prompt, int pwd_size);
void shell_loop(char* cmd, char* cwd_buf, int pwd_size,char* prompt);
int builtin_cmd(char **argv);
char* get_path(char* arg);
void copy_envp(char **local_envp,char** envp);
int fork_then_exec(char* path);
void free_paths(char** path);
void free_correct_args(char** correct_args);
int pipe_exec();
int pipe_exec_helper(char **correct_args,char* path, int first,int middle, int last,int fd[],int next_in);
int pipe_check(int pipe_c);
void signalHandler(int signal);
int pwd_forkexec();
void free_argv(char** argv);
void childHandler(int sig, siginfo_t *sip, void *notused);
int cmd_alarm(int n);
#endif
