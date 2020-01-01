/*
Description: System Programming Assignment, Smallsh
Language : C
Author: Doho Kim
Date:	2019.12.22
*/


#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#define EOL 1
#define ARG 2
#define AMPERSAND 3
#define SEMICOLON 4
#define PIPELINE 5

#define MAXARG 512
#define MAXBUF 512

#define FOREGROUND 0
#define BACKGROUND 1

static char special[] = {' ','\t', '&', ';', '\n', '\0'};

/*  프로그램 버퍼 및 작업용 포인터들 */
static char inpbuf[MAXBUF], tokbuf[2*MAXBUF], *ptr=inpbuf, *tok = tokbuf;
char *prompt = "prompt> ";
char *pipetok[10][MAXARG+1];

/* 함수 원형 선언 */
void catchsignal(int signo);
int inarg(char c);
int gettok(char ** outptr);
int userin(char *p);
int procline(void);
int runcommand(char **cline, int where);
int pipes(int npipe,int where);
int fatal(char *s);


/* 시그널 처리 함수 */
void catchsignal (int signo) {
	printf("\n");
	execl("./test", NULL);
}

/* 한 문자가 Special 문자인지 조사하는 함수 */
int inarg(char c)
{
	char *wrk;
	for(wrk = special; *wrk; wrk++)
	{
		if(c == *wrk)
			return (0);
	}

	return (1);
}

/* token을 얻는 함수 */
int gettok(char ** outptr) {
	int type;
	
	*outptr = tok;
	
	while(*ptr == ' ' || *ptr == '\t')
		ptr++;
	
	*tok++ = *ptr;

	switch(*ptr++) {
	case '\n':
		type = EOL;
		break;
	case '&':
		type = AMPERSAND;
		break;
	case ';':
		type = SEMICOLON;
		break;
	case '|':
		type = PIPELINE;
		break;
	default:
		type = ARG;

		while(inarg(*ptr))
			*tok++ = *ptr++;
	}

	*tok++ = '\0';
	return type;
}

/* 프롬프트를 프린트하고 한 줄을 읽는 함수 */
int userin(char *p)
{
	static struct sigaction act = { 0 };
	sigset_t mask1, mask2, oldset;
	
	sigemptyset(&mask1);
	sigemptyset(&mask2);

	sigaddset(&mask1, SIGINT);
	sigaddset(&mask2, SIGQUIT);

	act.sa_handler = catchsignal;
    
    sigaction(SIGINT, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
	
    sigprocmask(SIG_UNBLOCK, &mask1, &oldset);
	sigprocmask(SIG_UNBLOCK, &mask2, &oldset);
	
    char *quit = "quit\n";
	
	int c, count;
	ptr = inpbuf;
	tok = tokbuf;
	printf("%s", p);
	count = 0;
	
	
	while(1)
	{
		if((c=getchar())==EOF)
			return (EOF);
		if(count < MAXBUF)
			inpbuf[count++] = c;
		if(c=='\n' && count <MAXBUF)
		{
			if(strcmp(ptr, quit)==0){
				exit(1);
			}
			inpbuf[count] = '\0';
			return count;
		}
		if(c=='\n')
		{
			printf("smallsh: input line too long\n");
			count = 0;
			printf("%s", p);
		}
	}
}
/* gettok()를 이용하여 명령문을 분석하고 그 과정에서 인수의 목록을 작성*/
/* 개행문자나 세미콜론을 만나면 명령을 수행하기 위해 runcommand라 불리는 루틴을 호출*/
/* 입력은 userin을 통해 이미 읽어들였다고 가정*/
int procline(void)
{
	int ntype = 0;
	int b_pipe; //pipe 타입 여부
	int npipe = 0;

	char *arg[MAXARG + 1];
	int toktype;
	int narg;
	int type;
	
	narg = 0;
	
	int i, j;

	for(i=0; i<10; i++) {
		for(j=0; j<MAXARG+1; j++)
			pipetok[i][j] = 0;
	}

	for(;;)
	{
		switch(toktype = gettok(&arg[narg])) {
		case ARG:
			if(narg < MAXARG)
				narg++;
			break;
		case EOL:
		case SEMICOLON:
		case AMPERSAND:
			if(toktype == AMPERSAND)
                type = BACKGROUND;
            else
                type = FOREGROUND;
            if(narg != 0)
            {
                arg[narg] = NULL;
                
                if(b_pipe == PIPELINE) {
                    for(j=0; j<narg; j++)
                        pipetok[npipe][j] = arg[j];
                    pipes(npipe, type);
                }
                else {
                    runcommand(arg, type);
                }
            }
            if(toktype == EOL)
                return 0;
            narg = 0;
            break;
		case PIPELINE:
			for(i=0; i<narg; i++) {
				pipetok[npipe][i] = arg[i];
			}
			pipetok[npipe][narg] = NULL;
			narg=0;
            npipe++;
			b_pipe=toktype;
			break;
		}
	}
}

/* 명령을 수행하기 위한 모든 프로세스를 시작하게 함*/
/* where가 BACKGROUND로 지정되어 있으면 waitpid호출은 생략된다.*/
/* 그럴경우 runcommand는 단순히 프로세스 식별번호만 인쇄하고 복귀한다.*/
int runcommand(char **cline, int where)
{
	pid_t pid;
	int status;

	switch(pid=fork()) {
	case -1:
		perror("smallsh");
		return (-1);
	case 0:
		execvp(*cline, cline);
		perror(*cline);
		exit(1);
	}

	if(where==BACKGROUND)
	{
		printf("[Process id %d]\n", pid);
		return (0);
	}

	if(waitpid(pid, &status, 0) == -1)
		return (-1);
	else
		return (status);
}

/* pipe에 관해 처리하는 함수 */
int pipes(int npipe,int where)
{
	pid_t pid[npipe+1];
	int i=0,j;
	int p[npipe][2], status;
    
    /* 루틴의 나머지 부분으로 자식에 의해 수행된다 */
    
	/* 파이프를 만든다. */
	for(i=0;i<npipe;i++)
	{
		if(pipe(p[i])==-1)
			fatal("pipe call in join");
	}

	/* 다른 프로세스를 생성한다. */
	for(i=0;i<=npipe;i++)
	{
		switch(pid[i]=fork()){
        	case -1: /* 오류 */
                fatal("fork children failed");
        	case 0:
                /* 쓰는 프로세스 */
                if(i==0) {
                    dup2(p[i][1],1);    /* 표준 출력이 파이프로 가게 한다. */
                
                    for(j=0;j<npipe;j++) {
                        /* 화일 기술자를 절약한다*/
                        close(p[j][1]);
                        close(p[j][0]);
                    }
                    execvp(pipetok[i][0], pipetok[i]);
                    
                    /* execvp가 복귀하면, 오류가 발생한 것임*/
                    fatal(pipetok[i][0]);
                }

                else if (i==npipe) {
                    dup2(p[i-1][0],0);
                    for(j=0;j<npipe;j++) {
                        /* 화일 기술자를 절약한다*/
                        close(p[j][0]);
                        close(p[j][1]);
                    }

                    execvp(pipetok[npipe][0], pipetok[npipe]);
                    
                    /* execvp가 복귀하면, 오류가 발생한 것임*/
                    fatal(pipetok[npipe][0]);
                }

           		else {
                    /* 읽는 프로세스 */
                    
                    /* 표준 입력이 파이프로부터 오게 한다.*/
                    dup2(p[i-1][0],0);
                    dup2(p[i][1],1);

                    for(j=0;j<npipe;j++) {
                        close(p[j][0]);
                        close(p[j][1]);
                    }

                    execvp(pipetok[i][0],pipetok[i]);
                    
                    /* execvp가 복귀하면, 오류가 발생한 것임*/
                    fatal(pipetok[i][0]);
                }
        }

	}

	for(j=0;j<npipe;j++) {
		/* 화일 기술자를 절약한다*/
		close(p[j][0]);
		close(p[j][1]);
	}

    
	if (where==BACKGROUND) {
		for(j=0;j<=npipe;j++) {
			if (pid[j]>0)
               			printf("[Process id %d]\n",pid[j]);
            else{
                sleep(0);
            }
		}
		return(0);
	}
    
	while(waitpid(pid[npipe], &status, WNOHANG)==0)
		sleep(0);

	return(0);
}

int fatal(char *s) {
	perror(s);
	exit(1);
}

int main() {
	while(userin(prompt) != EOF) {
		procline();
	}
}
