#include "sfish.h"
#include "debug.h"
#include <stdlib.h>
#include <unistd.h>
/*
 * As in previous hws the main function must be in its own file!
 */
char* netid = "111";
char* prompt = NULL;
char *local_envp[100] = {0};
/*
    ISSUES LEFT
    3. PWD NO PIPING
    DONE:
    4. rpint to stderr done kind of
    1. pipe eerors DONE
    2. sig for child and arlm done
    3.  fix the help print statements ez DONE
    4. error with extra inputs in builtins DONE
	5. need to add envp that are correct DONE
    6. grep kevin < output.txt DONE
    7. error check for pipes
    8. child time DONE
*/
int main(int argc, char const *argv[], char* envp[]){
    /* DO NOT MODIFY THIS. If you do you will get a ZERO. */
    rl_catch_signals = 0;
    /* This is disable readline's default signal handlers, since you are going to install your own.*/
    copy_envp(local_envp,envp);
    char *cmd = NULL;
    int pwd_size = 1024;
    char *cwd_buf = malloc(pwd_size);
    prompt = malloc(pwd_size + 6);
    memset(prompt,0,pwd_size+6);
    char* begin_prompt = "<kkrajewski> <";
    char* end_prompt = "> ";
    strncat(prompt,begin_prompt,16);
    strncat(prompt,end_prompt,3);
	get_cwd(cwd_buf,prompt,pwd_size);//set default prompt
	set_prompt( pwd_size, prompt);
	signal(SIGTSTP , SIG_IGN); /**/
	signal(SIGUSR2 ,signalHandler);
    signal(SIGALRM ,signalHandler);

    struct sigaction action;
    action.sa_sigaction = childHandler; /* Note use of sigaction, not handler */
    sigfillset (&action.sa_mask);
    action.sa_flags = SA_SIGINFO; /* Note flag - otherwise NULL in function */

    sigaction (SIGCHLD, &action, NULL);
	/*start the loop*/
	/*split shit into tokens*/
	while((cmd = readline(prompt)) != NULL) {
    	if (cmd && *cmd)
    		add_history(cmd);//save this line?
    	fflush(stdout);
        alarm(alrm_set);
        process_cmd(cmd,pwd_size);
        current_dir = get_cwd(cwd_buf,prompt,pwd_size); //change prompt??
		set_prompt(pwd_size, prompt);
        free(cmd);


    }

    /* Don't forget to free allocated memory, and close file descriptors. */
   // free(cmd);
    free(cwd_buf);
    free(prompt);
    return EXIT_SUCCESS;
}
