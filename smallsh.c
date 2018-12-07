#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_CMD_ARG 10
#define MAX_CMD_GRP 10
#define MAX_CMD_LIST 10

const char* prompt = "myshell> ";

struct sigaction act;

char cmdline[BUFSIZ];
char* cmdgrps[MAX_CMD_GRP];
char* cmdvectors[MAX_CMD_LIST];
char* cmdargs[MAX_CMD_ARG];

void fatal(char* str);
void execute_cmdline(char* cmdline);
void execute_cmdgrp(char* cmdgrp);
void execute_cmdarg(char* cmdarg);

int makelist(char* s, const char* delimiters, char** list, int MAX_LIST);
int isBackground(char* s);

void parsing_redirect(char* cmdarg);

int cmd_exit(int argc, char* argv[]);
int cmd_cd(int argc, char* argv[]);
void zombie_handler(int sig);

int main(int argc, char **argv)
{
    int i = 0;

    //시그널 핸들러에 의해 중지된 시스템 호출을 자동적으로 재시작한다.
    act.sa_flags = SA_RESTART;
	sigemptyset(&act.sa_mask);
	act.sa_handler = zombie_handler;
	sigaction(SIGCHLD, &act, 0);
    
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    signal(SIGTTOU, SIG_IGN);
    
    
    while (1)
    {
        fputs(prompt, stdout);
        fgets(cmdline, BUFSIZ, stdin);
        cmdline[strlen(cmdline) - 1] = '\0';
        
        execute_cmdline(cmdline);
    }
    return 0;
}

void fatal(char *str)
{
    perror(str);
    return;
}

int makelist(char *s, const char *delimiters, char **list, int MAX_LIST)
{
    int i = 0;
    int numtokens = 0;
    char *snew = NULL;

    if ((s == NULL) || (delimiters == NULL))
        return -1;

    snew = s + strspn(s, delimiters); // delimiters를 skip 
    if ((list[numtokens] = strtok(snew, delimiters)) == NULL)
        return numtokens;

    numtokens = 1;

    while (1)
    {
        if ((list[numtokens] = strtok(NULL, delimiters)) == NULL)
            break;
        if (numtokens == (MAX_LIST - 1))
            return -1;
        numtokens++;
    }
    list[numtokens] = '\0';
    return numtokens;
}

void execute_cmdline(char* cmdline)
{
    int count = 0;
    int arg_count = 0;
    int i = 0;
    int pid;
    char cmdgrpstemp[BUFSIZ];
    int background;
    count = makelist(cmdline, ";", cmdgrps, MAX_CMD_GRP);
    for(i = 0; i < count; i++)
    {
        //makelist때문에 바뀌는 cmdgrps[i]를 위해 원본은 유지한다 + 1 은 널 때문이다
        memcpy(cmdgrpstemp, cmdgrps[i], strlen(cmdgrps[i]) + 1);
        background = isBackground(cmdgrpstemp);

        arg_count = makelist(cmdgrpstemp, " \t", cmdvectors, MAX_CMD_LIST);
        if(strcmp(cmdvectors[0], "cd") == 0)
        {
            cmd_cd(arg_count, cmdvectors);
            continue;
        }
        else if(strcmp(cmdvectors[0], "exit") == 0)
        {
            cmd_exit(arg_count, cmdvectors);
            return;
        }

        switch(pid = fork())
        {
            case -1:
                fatal("fork error");
                break;
            case 0:
                execute_cmdgrp(cmdgrps[i]);//cmdgrpstemp 대신 원본을 쓴다
            default:
                if(background == 0)
                {
                    waitpid(pid, NULL, 0);
                    tcsetpgrp(STDIN_FILENO, getpgid(0));
                }
                fflush(stdout);
        }
    }
}

int isBackground(char* s)
{
    int i;
    for(i = 0; i < strlen(s); i++)
    {
        if(s[i] == '&')
        {
            s[i] = ' ';
            return 1;
        }
    }
    return 0;
}

void execute_cmdgrp(char* cmdgrp)
{
    int i = 0;
    int count = 0;
    int background;
    int fd[2];

    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);

    setpgid(0, 0);

    background = isBackground(cmdgrp);
    if(background == 0)
        tcsetpgrp(STDIN_FILENO, getpgid(0));

    count = makelist(cmdgrp, "|", cmdvectors, MAX_CMD_LIST);
    //count = makelist(cmdgrp, " \t", cmdvector, MAX_CMD_ARG);
    for(i = 0; i < count - 1; i++)
    {
		pipe(fd);
		switch(fork())
		{
			case -1:
                fatal("fork error");
                break;
            case  0:
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                execute_cmdargs(cmdvectors[i]);
            default:
                close(fd[1]);
                dup2(fd[0], STDIN_FILENO);
		}
	}
	execute_cmdargs(cmdvectors[i]);

    //execvp(cmdvector[0], cmdvector);
    //fatal("exec error");
}

void execute_cmdargs(char* cmdarg)
{
    parsing_redirect(cmdarg);
    
    makelist(cmdarg, " \t", cmdargs, MAX_CMD_ARG);
	
    execvp(cmdargs[0], cmdargs);
    fatal("exec error");
}

int cmd_cd(int argc, char* argv[])
{
    if(argc == 1)
        chdir(getenv("HOME"));
    else if(argc == 2)
    {
        if(strcmp(argv[1], "~") == 0)
            chdir(getenv("HOME"));
        else if(chdir(argv[1]) == 1)
            fatal("chdir error");
    }
    else
        printf("use cd [directory]\n");

    return 0;
}

int cmd_exit(int argc, char* argv[])
{
    printf("my shell exit!\n");
    exit(0);

    return 0;
}

void zombie_handler(int sig)
{
    int status;
    int pid = waitpid(-1, &status, WNOHANG);
}

void parsing_redirect(char* cmdarg)
{
	char* arg;
	int fd;
    int i;
    
    for(i = strlen(cmdarg) - 1; i >= 0; i--)
	{
        if(cmdarg[i] == '<')
        {
            arg = strtok(&cmdarg[i + 1], " \t");
			if( (fd = open(arg, O_RDONLY | O_CREAT, 0644)) < 0) fatal("open error");
			dup2(fd, STDIN_FILENO);
			close(fd);
			cmdarg[i] = '\0';
        }
        else if(cmdarg[i] == '>')
        {
            arg = strtok(&cmdarg[i + 1], " \t");
            if( (fd = open(arg, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) fatal("open error");
            dup2(fd, STDOUT_FILENO);
            close(fd);
            cmdarg[i] = '\0';
        }
	}
    
}

