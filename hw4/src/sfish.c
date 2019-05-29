#include "sfish.h"
#include <stdlib.h>
#include <unistd.h>
char* previous_dir = NULL;
char* current_dir = NULL;
char *argv[100] = {0};
char *local_envp[100];
int max_argv = 0;
volatile sig_atomic_t alrm_set = 0;

void signalHandler(int signal)
{
	if (signal==SIGUSR2)
	{
		printf("\nWell that was easy.");
		fflush (stdout);
	}
	if(signal == SIGALRM)
	{
		printf("\nYour %d second timer has finished!\n", alrm_set);
		fflush (stdout);
	}
}
void childHandler(int sig, siginfo_t *sip, void *notused)
{
	long double cpu_time_used = (long double)(sip->si_utime + sip->si_stime) / ((long double)CLOCKS_PER_SEC);
	printf ("\nChild with PID %d has died. It spent %Lf milliseconds utilizing the CP\n", sip->si_pid,cpu_time_used);
    fflush (stdout);
}

void usage(void)
{
    printf("Usage: shell \n");
    printf("   help   print this message\n");
    printf("   pwd  print fhte current working directory\n");
    printf("   alarm [args]  sets a limit on how long a program can execute for\n");
    printf("   exit   exits the shell\n");
    printf("   cd [args]  change working directory\n");
}
void copy_envp(char **local_envp,char** envp)
{
	int i = 0;
	if(local_envp!= NULL)
	{
		for(;envp[i] != NULL; i++)
		{
			local_envp[i] = (char *)malloc(sizeof(char) * (strlen(envp[i]) + 1));
			memcpy(local_envp[i], envp[i], strlen(envp[i])+1);
		}
	}
}
int parse_cmd(char* line, int pwd_size, char** argv)
{
	int argc = 0,x=0;
	char local_cmd[pwd_size];
    char *ptr_cmd = local_cmd,*tok;
    strcpy(ptr_cmd, line);
    while(argv[x]!= NULL)
    {
    	free(argv[x]);
    	argv[x] = NULL;
    	x++;
    }
    tok = strtok(ptr_cmd," ");
	argv[argc] = (char *)malloc( sizeof(char) * (strlen(tok) + 1));
	bzero(argv[argc],(strlen(tok) + 1));
	strcpy(argv[argc++], tok);
    while(tok != NULL)
    {
    	tok = strtok(NULL," ");
    	if(tok != NULL)
    	{
			argv[argc] = (char *)malloc( sizeof(char) * (strlen(tok) + 1));
			bzero(argv[argc], (strlen(tok) + 1));
    		strcpy(argv[argc++], tok);
    	}
    }
    argv[argc] = NULL;
    if (argc == 0)	  /* ignore blank line */
		return 1;
    return 0;
}
int process_cmd(char* cmd, int pwd_size)
{
	if(strlen(cmd)==0)
	{
		return 0;
	}
    if((parse_cmd(cmd,pwd_size,&argv[0]))==1)
    	return 0;//empty line if its 1

    if(builtin_cmd(argv)==0)
    	return 0;//check for builins
    //no builtin was called
    if(index(cmd, '|') == NULL)
    {
    	if(index(argv[0], '/') == NULL) {
			char* path = get_path(argv[0]);
		    if(path != NULL)
		    {
		    	fork_then_exec(path);
		    	free(path);
		    }
		    else
		   	    fprintf(stderr,"%s: file not found/or cannot be openned\n",argv[0]);
	    }
	    else
	    {
	    	struct stat *buf;
			buf = malloc(sizeof(struct stat));//malloc
			errno = 0;
		    if(stat(argv[0],buf)==0)
		    {
		    	free(buf);//free
		    	fork_then_exec(argv[0]);
			}
			else
			    fprintf(stderr,"command not found\n");
	    }
	}
	else
	{
		pipe_exec();
	}
    //free local envp
    //
	return 0;
}
int pipe_check(int pipe_c)
{
	int i =0;
	int prg_c = 0;
	//int pipe_check = 0;
	if((strcmp(argv[0],"|"))==0)
	{
		//incorrect formating
		fprintf(stderr,"syntax error near unexpected token `|'\n");
		return -1;
	}
	while(argv[i] != NULL)
	{//count pipes
		if((strcmp(argv[i],"|"))==0)
		{
			pipe_c++;
		}
		i++;
	}
	//check if all programs can be openned
	//before anything else is don
	 i = 0;
	while(argv[i] != NULL)
	{
		if((strcmp(argv[i],"|"))==0)
		{
			//
			if(argv[i+1] == NULL)
			{
				fprintf(stderr,"invalid pipe arguments\n");
				return -1;
			}
		}
		else if(index(argv[i], '/') == NULL)
		{
			char* path = get_path(argv[i]);
		    if(path != NULL)//continue
		    {
		    	prg_c++;
		    	free(path);
		    }
	    }
	    else
	    {
	    	struct stat *buf;
			buf = malloc(sizeof(struct stat));//malloc
			errno = 0;
		    if(stat(argv[i],buf)==0)
		    {
		    	prg_c++;
		    	free(buf);//free
		    }
	    }
		i++;
	}
	//now check the program of programs is correct
	//to the amount of pipes in the cmd
	return pipe_c;
}

int pipe_exec_helper(char **correct_args,char* path, int first,int middle, int last,int fd[],int next_in)
{
	//assumes correct args are given for a program
	//assumes the apth given to the program is corerct also
	pid_t pid;
	pid = fork();
	if (pid == 0)
    {
    	if(first)
    	{
    		dup2(fd[1], 1); /* this end of the pipe becomes the standard output */
    		close(next_in); /* this process don't need the other end */

    	}
    	if(middle)
    	{
    		dup2(next_in, 0);//read end of the first one
	  		dup2(fd[1], 1); //read end of the second pipe

    	}
    	if(last)
    	{
    		dup2(next_in, 0); /* this end of the pipe becomes the standard input */
    		close(fd[1]); /* this process doesn't need the other end */
    	}
   	    setenv("parent",getcwd(current_dir, 1024),1);
		if (execve(path,correct_args,local_envp) == -1)
	    	perror("invalid directory");
	}
	return fd[0];//old read
}
int pipe_exec()
{
	int i = 0, j = 0,pipe_count = 0,
	current_pipe = 0,next_pipe = 0;
	char *correct_args[100] = {0};
	int pid, status;
	if((pipe_count = pipe_check(pipe_count)) == -1)
		return -1;
	int fd[2];
	int first,last,middle;
	first = 1;
	middle = 0;
	last = 0;
	int next_in = 0;
	/*  1.copy argv
		2. check how many valid ones are left
		3. then send that to the pipe
		4. if only one is left then fork and leave pipe_exec
	*/
	char *temp_argv[100];
	int copy = 0;
	while(argv[copy] != NULL)
	{
		temp_argv[copy] = (char *)malloc(sizeof(char) * (strlen(argv[copy]) + 1));
		memcpy(temp_argv[copy], argv[copy], strlen(argv[copy])+1);
		copy++;
	}
	temp_argv[copy] = NULL;
	copy = 0;
	while(temp_argv[copy] != NULL)
	{
		copy++;
	}
	//2. now check for valid ones
	int first_arg = 1;
	copy = 0;
	while(argv[copy] != NULL)
	{
		if((strcmp(argv[copy],"|"))==0)
		{
			first_arg = 1;
		}
		else if(index(argv[copy], '/') == NULL && first_arg)
		{
			char* path = get_path(argv[copy]);
		    if(path == NULL)//continue
		    {
		    	//set to null?
		    }
		    free(path);
	    }
	    else if(first_arg)
	    {
	    	struct stat *buf;
			buf = malloc(sizeof(struct stat));//malloc
			errno = 0;
		    if(stat(argv[copy],buf)!=0)
		    {
		    	//set to null?
		    	free(buf);//free
		    }
	    }
		copy++;
	}
	//free temp
	char** temp_path = temp_argv;
	char* path_ptr;
	while(*temp_path != NULL)
	{
		path_ptr = *temp_path;
		free(path_ptr);
		temp_path++;
		//path_ptr = NULL;
	}

	///int change_first = 0;
	while(current_pipe < pipe_count)
	{
		i = 0;
		while(argv[j] != NULL && !next_pipe)
	    {//send the right args to exec program
	    	if(((strcmp(argv[j],"|"))==0))//keep j and dont reset
				next_pipe = 1;
			else
			{
	    		correct_args[i] = (char *)malloc(sizeof(char) * (strlen(argv[j]) + 1));
	    		strcpy(correct_args[i],argv[j]);
	    		i++;
			}
	    	j++;
	    }
	    pipe( fd ); //PIPE CODE
	    next_pipe = 0;
	    correct_args[i] = NULL;
	    if(index(correct_args[0], '/') == NULL)
	    {
			char* path = get_path(correct_args[0]);
		   // if(path == NULL)//continue
		    //	return -1;
		    //else
		   // {
		   	if(path == NULL)
		   	{
		   		fprintf(stderr,"%s: command not found\n",correct_args[0]);
		   		//pipe_exec_helper(correct_args,correct_args[0],first,middle,last,fd,next_in);
		   		//change_first = 1;
		   	}
		   	else
		    	pipe_exec_helper(correct_args,path,first,middle,last,fd,next_in); //PIPE COIDE
		   	if(path != NULL)
		   		free(path);
		   // }
	    }
	    else
	    {
	    	struct stat *buf;
			buf = malloc(sizeof(struct stat));//malloc
			errno = 0;
		    if(stat(correct_args[0],buf)==0)
		    {
		    	pipe_exec_helper(correct_args,correct_args[0],first,middle,last,fd,next_in);

		    }
			else
			{
				fprintf(stderr,"%s: command not found\n",correct_args[0]);

			}
			free(buf);
	    }
	    free_correct_args(correct_args);
	    current_pipe++;
	    first = 0;
	    middle = 1;
	    next_in = fd[0];
	    close(fd[1]);
	}
	//now handle the last inthe piping
    next_pipe = 0;
    i = 0; // keep j tho
    while(argv[j] != NULL && !next_pipe)
	{//send the right args to exec program
    	if(((strcmp(argv[j],"|"))==0))
			next_pipe = 1;
    	else
    	{
    		correct_args[i] = (char *)malloc(sizeof(char) * (strlen(argv[j]) + 1));
	   		strcpy(correct_args[i],argv[j]);
	   		i++;
		}
		j++;
    }
    //last
    correct_args[i] = NULL;
    first = 0;
	middle = 0;
	last = 1;
    if(index(correct_args[0], '/') == NULL) {
		char* path = get_path(correct_args[0]);

	    if(path != NULL)//continue
	    {
	    	pipe_exec_helper(correct_args,path,first,middle,last,fd,next_in);//PIPE CODE
	    	free(path);
	    }
	    else
	    {
	    	fprintf(stderr,"%s: command not found\n",correct_args[0]);
	    	//return -1;
	    }
	    free_correct_args(correct_args);
    }
    else
    {
    	struct stat *buf;
		buf = malloc(sizeof(struct stat));//malloc
		errno = 0;
	    if(stat(correct_args[0],buf)==0)
	    {
	    	pipe_exec_helper(correct_args,correct_args[0],first,middle,last,fd,next_in);
	    	//free
	    }
		else
		{
			fprintf(stderr,"%s: command not found\n",correct_args[0]);
			//return -1;
		}
		free(buf);
		free_correct_args(correct_args);
    }
    close(fd[0]);//PIPE CODE
    close(fd[1]);//PIPE CODE
    while ((pid = wait(&status)) != -1);
    /* pick up all the dead children */
    	//fprintf(stderr, "process %d exits with %d\n", pid, WEXITSTATUS(status));
	return 1;
}

int fork_then_exec(char* path)
{
	pid_t pid, wpid;
	int status;
	pid = fork();
	/*
		    prog1 [ARGS] > output.txt //we good bois
			prog1 [ARGS] < input.txt //we good bois
			prog1 [ARGS] < input.txt > output.txt ??? maybe
	*/
    if (pid == 0)
    {// Child process
    	int in = 0,out = 0,i = 0;
	    char *input = NULL,*output = NULL;
	    int invalid_redir = 0;
		while(argv[i] != NULL)
		{
			if((strcmp(argv[i],"<"))==0)
			{
				in = 1;
				input = argv[i+1];
			}
			if((strcmp(argv[i],">"))==0)
			{
				out = 1;
				output = argv[i+1];
			}
			i++;
		}
		if(invalid_redir)
			perror("invalid redirection order given");
    	if(in)
	    {
	        int fd0;
	        if ((fd0 = open(input, O_RDONLY, 0)) < 0)
	        {
	            perror("cannot open input file");
	            exit(0);
	        }
	        dup2(fd0, 0);
	        close(fd0);
	    }
	    if(out)
	    {
	         int fd1;
	         if ((fd1 = creat(output , 0644)) < 0)
	         {
	            perror("cannot open output file");
	            exit(0);
	        }
	        dup2(fd1, STDOUT_FILENO);
        	close(fd1);
	    }
	   // setenv("parent",getcwd(current_dir, 1024),1);
	    char *correct_args[100] = {0};
	    if(in || out)
	    {
	    	int j = 0;
		    while(argv[j]!=NULL)
		    {//send the right args to exec program
		    	if(((strcmp(argv[j],">"))==0) || (((strcmp(argv[j],"<"))==0)))
		    	{
					break;
		    	}
				else
				{
		    		correct_args[j] = (char *)malloc(sizeof(char) * (strlen(argv[j]) + 1));
		    		strcpy(correct_args[j],argv[j]);
				}
		    	j++;
		    }
		    correct_args[j] = NULL;
		    if (execve(path,correct_args,local_envp) == -1)
	    		perror("cannot make child");
	    }
	    else
	    {
	    	if (execve(path,argv,local_envp) == -1)
	    		perror("cannot make child");
	    }
	    free_correct_args(correct_args);
    }
    else if (pid < 0)
    	perror("error in fork");// Error forking
    else
    {
    	do// Parent process
    	{
    		wpid = waitpid(pid, &status, WUNTRACED);
    		if(wpid){}
    		//exit on signal or if terminated normally
    	}while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
     //free_correct_args(correct_args);
    //free correct args
	return 1;
}
int builtin_cmd(char **argv)
{
    if((strcmp(argv[0],"exit"))==0)
    {
    	if( argv[1]!= NULL )
			fprintf(stderr,"invalid args in help\n");
		else
		{
			free_argv(argv);
			exit(0);
		}
    }
    else if((strcmp(argv[0],"alarm"))==0)
	{
		if(argv[1] == NULL)
		{
			fprintf(stderr,"invalid args in alarm\n");
		}
		else if( *(argv[1]+1) != '\0')
		{
			fprintf(stderr,"invalid args in alarm\n");
		}
		else if( isdigit(*(argv[1])))
		{
			cmd_alarm( *(argv[1]) );
		}
		else
		{
			fprintf(stderr,"invalid alarm args\n");
		}
	}
	else if((strcmp(argv[0],"cd"))==0)
	{
		if(argv[2]!= NULL)
			fprintf(stderr,"invalid args in cd\n");
		else
			cmd_cd(argv[1]);
	}
	else if((strcmp(argv[0],"help"))==0)
	{
		if(argv[1]!= NULL )
			fprintf(stderr,"invalid args in help\n");
		else
			usage();
	}
	else if((strcmp(argv[0],"pwd"))==0)
	{
		if(argv[1] != NULL)
		{
			if( !((strcmp(argv[1],"<"))==0) && !((strcmp(argv[1],">"))==0)
			&& !((strcmp(argv[1],"|"))==0))
				fprintf(stderr,"invalid args in pwd\n");
			else
				pwd_forkexec();
		}
		else
			pwd_forkexec();
	}
    else
		return 1;     /* not a builtin command */
    return 0;
}
int cmd_alarm(int n)
{
	signal(SIGALRM, signalHandler);
	n = n - 48;
	alrm_set = n;
	alarm(n);
	return 1;
}
int pwd_forkexec()
{
	//put a fork and exec and do redirection also
		//
		pid_t pid, wpid;
		int status;
		pid = fork();
    	if (pid == 0)
    	{
	    	int out = 0,i = 0;
		    char *output = NULL;
		    int invalid_redir = 0;
			while(argv[i] != NULL)
			{
				if((strcmp(argv[i],">"))==0)
				{
					out = 1;
					output = argv[i+1];
				}
				if((strcmp(argv[i],"|"))==0)
				{
					char* cwd = getcwd(NULL,0);
			    	printf("%s\n",cwd);
			    	_exit(0);
				}
				i++;
			}
			if(invalid_redir)
				fprintf(stderr,"invalid redirection order given\n");
		    if(out)
		    {
		         int fd1;
		         if ((fd1 = creat(output , 0644)) < 0)
		         {
		            fprintf(stderr,"cannot open output file\n");
		            exit(0);
		        }
		        dup2(fd1, STDOUT_FILENO);
	        	close(fd1);
		    }
		    setenv("parent",getcwd(current_dir, 1024),1);
	    	char* cwd = getcwd(NULL,0);
	    	printf("%s\n",cwd);
	    	_exit(0);

    	}
	    else if (pid < 0)
	    	fprintf(stderr,"error in fork\n");// Error forking
	    else
	    {
	    	do// Parent process
	    	{
	    		wpid = waitpid(pid, &status, WUNTRACED);
	    		if(wpid){}
	    		//exit on signal or if terminated normally
	    	}while (!WIFEXITED(status) && !WIFSIGNALED(status));
	    }
	    return 1;
}

char* get_path(char* arg)
{
	if(arg == NULL)
	{
		return 0;
	}
	char *paths[64] = {0};
	char* pr = getenv("PATH");
	char* local_path = (char *)malloc( sizeof(char) * (strlen(pr) + 1));
	strcpy(local_path,pr);
    char* tok;
    tok = strtok(local_path,":");
    int i = 0;
	paths[i] = (char *)malloc( sizeof(char) * (strlen(tok) + 1));
	strcpy(paths[i++], tok);
	while(tok!= NULL)
	{
		tok = strtok(NULL,":");
    	if(tok != NULL)
    	{
    		paths[i] = (char *)malloc( sizeof(char) * (strlen(tok) + 1));
    		strcpy(paths[i++], tok);
    	}
	}
	paths[i] = NULL;
	i = 0;
	int buf_len;// = strlen(paths[0]) + 2 + strlen(arg);//argv[0]
	char* buffer = NULL;//= malloc(buf_len + 1);
	while(paths[i] != NULL)
	{
		if(i == 0)
		{
			buf_len = strlen(paths[0]) + 2 + strlen(arg);//argv[0]
			buffer = malloc(buf_len + 1);
			memset(buffer,0,buf_len + 1);
		}
		else
		{
			buf_len = strlen(paths[i]) + 2 + strlen(arg);//argv[0]
			buffer = realloc(buffer,buf_len + 1);
			memset(buffer,0,buf_len + 1);
		}
		strcat(buffer, paths[i]);
		strcat(buffer,"/");
		strcat(buffer,arg);//argv[0]
		if(access(buffer, X_OK) == 0)
		{
			free_paths(paths);
			free(local_path);
			return buffer;
		}
		else
			memset(buffer,0,strlen(buffer));
		i++;
	}
	free_paths(paths);
	free(local_path);
	free(buffer);
	return NULL;
}
void free_paths(char** path)
{
	char** temp_path = path;
	char* path_ptr;
	while(*temp_path != NULL)
	{
		path_ptr = *temp_path;
		free(path_ptr);
		temp_path++;
	}
	return;
}
void free_correct_args(char** correct_args)
{
	char** temp_path = correct_args;
	char* path_ptr;
	while(*temp_path != NULL)
	{
		path_ptr = *temp_path;
		free(path_ptr);
		temp_path++;
		//path_ptr = NULL;
	}
	return;
}
void free_argv(char** argv)
{
	char** temp_path = argv;
	char* path_ptr;
	while(*temp_path != NULL)
	{
		path_ptr = *temp_path;
		free(path_ptr);
		temp_path++;
		//path_ptr = NULL;
	}
	return;
}
int cmd_cd(char* word)
{
	char *value;
	int need_to_free = 0;
	if(word == NULL)
	{
		if(previous_dir == NULL)
			previous_dir = malloc(sizeof(char) * (strlen(current_dir) + 1));
		else
			previous_dir = realloc(previous_dir,sizeof(char) * (strlen(current_dir) + 1));
		strcpy(previous_dir,current_dir);
    	value = getenv("HOME");
	}
    else if(strcmp(word, ".") == 0 || strcmp(word,"..")==0)
    {
    	if(previous_dir == NULL)
			previous_dir = malloc(sizeof(char) * (strlen(current_dir) + 1));
		else
			previous_dir = realloc(previous_dir,sizeof(char) * (strlen(current_dir) + 1));
		strcpy(previous_dir,current_dir);
    	value = word;
    }
    else if(strcmp(word, " ") == 0)
    {
    	if(previous_dir == NULL)
			previous_dir = malloc(sizeof(char) * (strlen(current_dir) + 1));
		else
			previous_dir = realloc(previous_dir,sizeof(char) * (strlen(current_dir) + 1));
		strcpy(previous_dir,current_dir);
    	value = getenv("HOME");
    }
    else if(strcmp(word, "-") == 0)
    {
    	if(previous_dir != NULL)
    	{
    		value = malloc(sizeof(char) * (strlen(previous_dir) + 1));; //get the new dir if not null
    		strcpy(value,previous_dir);
    		need_to_free = 1;
    		previous_dir = realloc(previous_dir,sizeof(char) * (strlen(current_dir) + 1));
    		strcpy(previous_dir,current_dir);
    	}
    	else
    	{
    		fprintf(stderr,"there is no previous dir\n");
    		return 1;
    	}
    }
    else
    	value=word;
	if (chdir (value) == -1)
    {
        perror(word);
        return 1;
    }
    int overwrite = 1;//update the env
	setenv("PWD", value, overwrite);
    if(need_to_free == 1)
    	free(value);
	return 0;
}
char* get_cwd(char* cwd_buf,char* prompt, int pwd_size)
{
	int enough_pwd_space = 0;
    while(enough_pwd_space == 0)
	{
		current_dir = getcwd(cwd_buf, (size_t)pwd_size);
	    if(current_dir != NULL)
	    	enough_pwd_space = 1;
	    else// resize
	    {
	    	enough_pwd_space = 0;
	    	pwd_size = pwd_size*2; //double each time till its enough
      		cwd_buf = realloc(cwd_buf,pwd_size);
      		prompt = realloc(cwd_buf,pwd_size + 6);
	    }
	}
	return current_dir;
}
void set_prompt(int prompt_space, char* prompt){
	int id_len = 0;
	    while(prompt[id_len] != ' ')
	    	id_len++; // move to empty space
	    id_len++; // move to '<'
	    id_len++; //move to right after '<'
	    int y = id_len;
	    while(prompt[y] !='\0')
	    	y++;
	    y--; // move y to ' ' at the end
	    y--; // move to '>'
	    int new_prompt_len = strlen(current_dir);
	    int cur_prompt_len = prompt_space + 6;
	    int i = 0;
	    while(i < cur_prompt_len)
	    {
	    	if(i < new_prompt_len)
	    		prompt[id_len + i] = current_dir[i]; // start to replace shit
	    	if(i == new_prompt_len)
	    	{
	    		prompt[id_len + i++] = '>';
	    		prompt[id_len + i++] = ' ';
	    		prompt[id_len + i++] = '\0';
	    		break;
	    	}
	    	i++;
	    }
}
