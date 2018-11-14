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
// #define BUF_SIZE 512

const char *prompt = "myshell> ";

struct sigaction act;

char* cmdgrps[MAX_CMD_GRP];
char* cmdvector[MAX_CMD_ARG];
char cmdline[BUFSIZ];
void fatal(char* str);
void execute_cmdline(char* cmdline);
void execute_cmdgrp(char* cmdgrp);
//int makelist(char* s, const char* delimiters, char** list, int MAX_LIST);
int makeargv(char* s, const char* delimiters, char** list, int MAX_LIST);
int isBackground(char* s);
int cmd_exit(int argc, char* argv[]);
int cmd_cd(int argc, char* argv[]);
void read_childproc(int sig);

int main(int argc, char **argv)
{
    int i = 0;
    //pid_t pid;

    act.sa_flags = SA_RESTART; //시그널 핸들러에 의해 중지된 시스템 호출을 자동적으로 재시작한다.
    //act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	act.sa_handler = read_childproc;
	sigaction(SIGCHLD, &act, 0);

    while (1)
    {
        fputs(prompt, stdout);
        fgets(cmdline, BUFSIZ, stdin);
        cmdline[strlen(cmdline) - 1] = '\0';
        /*
        switch (pid = fork())
        {
        case 0:
            makelist(cmdline, " \t", cmdvector, MAX_CMD_ARG);
            execvp(cmdvector[0], cmdvector);
            fatal("main()");
        case -1:
            fatal("main()");
        default:
            wait(NULL);
        }
        */
       execute_cmdline(cmdline);
       printf("\n");
    }
    return 0;
}

void fatal(char *str)
{
    perror(str);
    exit(1);
}
/*
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
    return numtokens;
}
*/
int makeargv(char* s, const char* delimiters, char** list, int MAX_LIST)
{
    int i = 0;
    int numtokens = 0;
    char *snew = NULL;

    if ((s == NULL) || (delimiters == NULL))
        return -1;

    snew = s + strspn(s, delimiters); /* delimiters를 skip */
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
    count = makeargv(cmdline, ";", cmdgrps, MAX_CMD_GRP);//makelist
    for(i = 0; i < count; i++)
    {
        //makeargv때문에 바뀌는 cmdgrps[i]를 위해 원본은 유지한다 + 1 은 널 때문에
        memcpy(cmdgrpstemp, cmdgrps[i], strlen(cmdgrps[i]) + 1);
        background = isBackground(cmdgrpstemp);
        arg_count = makeargv(cmdgrpstemp, " \t", cmdvector, MAX_CMD_ARG);
        if(strcmp(cmdvector[0], "cd") == 0)
        {
            cmd_cd(arg_count, cmdvector);
            continue;
        }
        else if(strcmp(cmdvector[0], "exit") == 0)
        {
            cmd_exit(arg_count, cmdvector);
            return;
        }
        
        switch(pid = fork())
        {
            case -1: fatal("fork error");
            case 0:
                execute_cmdgrp(cmdgrps[i]);//execute_cmdgrp(cmdgrpstemp) 대신
            default:
                if(background == 0)
                    waitpid(pid, NULL, 0);
                    //break;
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

    background = isBackground(cmdgrp);

    count = makeargv(cmdgrp, " \t", cmdvector, MAX_CMD_ARG);
    execvp(cmdvector[0], cmdvector);
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
        else if(chdir(argv[1]) == -1)
            fatal("chdir error");
    }
    else
        printf("use cd [directory]\n");

    return 0;
}

int cmd_exit(int argc, char* argv[])
{
    printf("exit!\n");
    exit(0);

    return 0;
}

void read_childproc(int sig)
{
    int status;
    int pid = waitpid(-1, &status, WNOHANG);

    /*
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
        printf("child %d terminated normaly\n", pid) ;
    */

    if(pid > 0)
    {
        printf("background process child %d terminated normaly\n", pid) ;
        fputs(prompt, stdout);
        fflush(stdout);
        //printf("%s", prompt);
    }
}