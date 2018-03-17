/*
 * OS Assignment #1
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>//srand함수를 사용하기 위한 헤더 파일
#include <sys/wait.h>
#include <sys/signalfd.h>//signalfd를 사용하기 위한 헤더 파일
#define MSG(x...) fprintf (stderr, x)
#define STRERROR  strerror (errno)

#define ID_MIN 2
#define ID_MAX 8
#define COMMAND_LEN 256

typedef enum
{
	ACTION_ONCE,
	ACTION_RESPAWN,

} Action;

typedef struct _Task Task;
struct _Task
{
	Task          *next;

	volatile pid_t pid;
	int            piped;
	int            pipe_a[2];
	int            pipe_b[2];
	//====================================Edit Part=================================================
	int            order;//order항목 추가
	//=============================================================================================
	char           id[ID_MAX + 1];
	char           pipe_id[ID_MAX + 1];
	Action         action;
	char           command[COMMAND_LEN];
};

static Task *tasks;

static volatile int running;

static char *
strstrip(char *str)
{
	char  *start;
	size_t len;

	len = strlen(str);
	while (len--)
	{
		if (!isspace(str[len]))
			break;
		str[len] = '\0';
	}

	for (start = str; *start && isspace(*start); start++)
		;
	memmove(str, start, strlen(start) + 1);

	return str;
}

static int
check_valid_id(const char *str)
{
	size_t len;
	int    i;

	len = strlen(str);
	if (len < ID_MIN || ID_MAX < len)
		return -1;

	for (i = 0; i < len; i++)
		if (!(islower(str[i]) || isdigit(str[i])))
			return -1;

	return 0;
}
//====================================Edit Part========================================================
static int
check_valid_order(const char *str)//order가 4자리를 넘어가거나, 숫자가 아닐 경우 예외처리를 위한 함수
{
	size_t len;
	int    i;

	len = strlen(str);//문자열의 길이 측정
	if (len > 4)//만약 4자리수를 넘어 갈경우 
		return -1;//-1반환하여 유효하지 않음을 반환

	for (i = 0; i < len; i++)//4자리수 이내의 경우일지라도
		if (str[i] < '0' || str[i] > '9')//0~9사이의 숫자인지 확인함
			return -1;//숫자가 아닌 문자가 들어있을 경우 유효하지 않음을 반환

	return 0;//아무 문제 없을시 0 반환
}//====================================================================================================

static Task *
lookup_task(const char *id)
{
	Task *task;

	for (task = tasks; task != NULL; task = task->next)
		if (!strcmp(task->id, id))
			return task;

	return NULL;
}

static Task *
lookup_task_by_pid(pid_t pid)
{
	Task *task;

	for (task = tasks; task != NULL; task = task->next)
		if (task->pid == pid)
			return task;

	return NULL;
}

static void
append_task(Task *task)
{
	Task *new_task;
	Task *temp;

	new_task = malloc(sizeof(Task));
	if (!new_task)
	{
		MSG("failed to allocate a task: %s\n", STRERROR);
		return;
	}

	*new_task = *task;
	new_task->next = NULL;
	//======================================Edit Part================================================
	if (!tasks)
		tasks = new_task;
	else
	{
		Task *t;
		if (new_task->order < tasks->order)//new_task의 오더가 head의 오더보다 작을경우 head 앞에 붙임
		{
			new_task->next = tasks;//new_task 다음 노드를 tasks로 설정
			tasks = new_task;//head를 new_task로 변경
		}
		else if (new_task->order >= tasks->order)//만약 헤드의 order값이 new_task의 order값보다 클경우
		{
			t = tasks;
			while (t != NULL && new_task->order >= t->order)//자신보다 최초로 같거나 큰order값을 찾은 후 해당 
			{                                              //노드의 바로 앞에 삽입함
				temp = t;
				t = t->next;
			}
			new_task->next = temp->next;//링크드리스트 재연결
			temp->next = new_task;//연결 완료
		}
	}//append_task를 변경하여 자동으로 order순으로 오름차순 정렬되는 링크드리스트를 만들었습니다.
}//====================================================================================================

static int
read_config(const char *filename)
{
	FILE *fp;
	char  line[COMMAND_LEN * 2];
	int   line_nr;

	fp = fopen(filename, "r");
	if (!fp)
		return -1;

	tasks = NULL;

	line_nr = 0;
	while (fgets(line, sizeof(line), fp))
	{
		Task   task;
		char  *p;
		char  *s;
		size_t len;

		line_nr++;
		memset(&task, 0x00, sizeof(task));

		len = strlen(line);
		if (line[len - 1] == '\n')
			line[len - 1] = '\0';

		if (0)
			MSG("config[%3d] %s\n", line_nr, line);

		strstrip(line);

		/* comment or empty line */
		if (line[0] == '#' || line[0] == '\0')
			continue;

		/* id */
		s = line;
		p = strchr(s, ':');
		if (!p)
			goto invalid_line;
		*p = '\0';
		strstrip(s);
		if (check_valid_id(s))
		{
			MSG("invalid id '%s' in line %d, ignored\n", s, line_nr);
			continue;
		}
		if (lookup_task(s))
		{
			MSG("duplicate id '%s' in line %d, ignored\n", s, line_nr);
			continue;
		}
		strcpy(task.id, s);

		/* action */
		s = p + 1;
		p = strchr(s, ':');
		if (!p)
			goto invalid_line;
		*p = '\0';
		strstrip(s);
		if (!strcasecmp(s, "once"))
			task.action = ACTION_ONCE;
		else if (!strcasecmp(s, "respawn"))
			task.action = ACTION_RESPAWN;
		else
		{
			MSG("invalid action '%s' in line %d, ignored\n", s, line_nr);
			continue;
		}
		//===================================Edit Part===================================================
		/* Order */
		s = p + 1;//s = p + 1 을 통해서 다음 문자열 위치로 넘겨 줍니다.
		p = strchr(s, ':');// :에 해당하는 문자열 포인터를 찾아 p에 입력합니다.
		if (!p)//만약 포맷이 잘못됬을 경우 invalid_line 분기로 goto 시킵니다.
		{
			goto invalid_line;
		}
		*p = '\0';// :에 해당하는 부분을 널 문자로 바꿔줍니다.
		strstrip(s);//:가 널문자로 변경되어 strstrip을 통한 파싱이 가능합니다.
		if (check_valid_order(s))//최종적으로 나온 order 문자열을 검사하는 함수입니다.
		{
			MSG("invalid order '%s' in line %d, ignored\n", s, line_nr);//유효하지 않은 값일경우 오류 메세지 출력
			continue;
		}
		if (strlen(s) == 0)//공백일 경우 임의의 순서를 정합니다.
			task.order = rand() % 100 + 10000;//오더는 무조건 0~9999사이의 숫자이기 때문에 rand() % 100 + 10000을
											  //이용하면 order가 공백이 아닌 task보다는 순서가 무조건 뒤로 가되,
		else                                  //임의의 순서로 실행되도록 구현할 수 있습니다.
			task.order = atoi(s);//공백도 아니고 유효성 검사도 통과했다면 문자열을 숫자로 변환하여 저장
		//===============================================================================================
		/* pipe-id */
		s = p + 1;
		p = strchr(s, ':');
		if (!p)
			goto invalid_line;
		*p = '\0';
		strstrip(s);
		if (s[0] != '\0')
		{
			Task *t;

			if (check_valid_id(s))
			{
				MSG("invalid pipe-id '%s' in line %d, ignored\n", s, line_nr);
				continue;
			}

			t = lookup_task(s);
			if (!t)
			{
				MSG("unknown pipe-id '%s' in line %d, ignored\n", s, line_nr);
				continue;
			}
			if (task.action == ACTION_RESPAWN || t->action == ACTION_RESPAWN)
			{
				MSG("pipe not allowed for 'respawn' tasks in line %d, ignored\n", line_nr);
				continue;
			}
			if (t->piped)
			{
				MSG("pipe not allowed for already piped tasks in line %d, ignored\n", line_nr);
				continue;
			}

			strcpy(task.pipe_id, s);
			task.piped = 1;
			t->piped = 1;
		}

		/* command */
		s = p + 1;
		strstrip(s);
		if (s[0] == '\0')
		{
			MSG("empty command in line %d, ignored\n", line_nr);
			continue;
		}
		strncpy(task.command, s, sizeof(task.command) - 1);
		task.command[sizeof(task.command) - 1] = '\0';

		if (0)
			MSG("id:%s pipe-id:%s action:%d command:%s\n",
				task.id, task.pipe_id, task.action, task.command);

		append_task(&task);
		continue;

	invalid_line:
		MSG("invalid format in line %d, ignored\n", line_nr);
	}

	fclose(fp);

	return 0;
}

static char **
make_command_argv(const char *str)
{
	char      **argv;
	const char *p;
	int         n;

	for (n = 0, p = str; p != NULL; n++)
	{
		char *s;

		s = strchr(p, ' ');
		if (!s)
			break;
		p = s + 1;
	}
	n++;

	argv = calloc(sizeof(char *), n + 1);
	if (!argv)
	{
		MSG("failed to allocate a command vector: %s\n", STRERROR);
		return NULL;
	}

	for (n = 0, p = str; p != NULL; n++)
	{
		char *s;

		s = strchr(p, ' ');
		if (!s)
			break;
		argv[n] = strndup(p, s - p);
		p = s + 1;
	}
	argv[n] = strdup(p);

	if (0)
	{

		MSG("command:%s\n", str);
		for (n = 0; argv[n] != NULL; n++)
			MSG("  argv[%d]:%s\n", n, argv[n]);
	}

	return argv;
}

static void
spawn_task(Task *task)
{
	if (0) MSG("spawn program '%s'...\n", task->id);

	if (task->piped && task->pipe_id[0] == '\0')
	{
		if (pipe(task->pipe_a))
		{
			task->piped = 0;
			MSG("failed to pipe() for prgoram '%s': %s\n", task->id, STRERROR);
		}
		if (pipe(task->pipe_b))
		{
			task->piped = 0;
			MSG("failed to pipe() for prgoram '%s': %s\n", task->id, STRERROR);
		}
	}

	task->pid = fork();
	if (task->pid < 0)
	{
		MSG("failed to fork() for program '%s': %s\n", task->id, STRERROR);
		return;
	}

	/* child process */
	if (task->pid == 0)
	{
		char **argv;

		argv = make_command_argv(task->command);
		if (!argv || !argv[0])
		{
			MSG("failed to parse command '%s'\n", task->command);
			exit(-1);
		}

		if (task->piped)
		{
			if (task->pipe_id[0] == '\0')
			{
				dup2(task->pipe_a[1], 1);
				dup2(task->pipe_b[0], 0);
				close(task->pipe_a[0]);
				close(task->pipe_a[1]);
				close(task->pipe_b[0]);
				close(task->pipe_b[1]);
			}
			else
			{
				Task *sibling;

				sibling = lookup_task(task->pipe_id);
				if (sibling && sibling->piped)
				{
					dup2(sibling->pipe_a[0], 0);
					dup2(sibling->pipe_b[1], 1);
					close(sibling->pipe_a[0]);
					close(sibling->pipe_a[1]);
					close(sibling->pipe_b[0]);
					close(sibling->pipe_b[1]);
				}
			}
		}

		execvp(argv[0], argv);
		MSG("failed to execute command '%s': %s\n", task->command, STRERROR);
		exit(-1);
	}
}

static void
spawn_tasks(void)
{
	Task *task;
	//===================================Edit Part====================================================
	for (task = tasks; task != NULL && running; task = task->next)
	{
		spawn_task(task);
		usleep(1000000);//CPU의 멀티 프로세싱때문인지, 분명히 실행 순서는 링크드리스트의
		                //Order순인데도 불구하고, 자꾸 순서가 뒤죽박죽으로 출력되어
		                //usleep(1000000)을 통해서 spawn_task간의 딜레이를 주었더니 정상적으로 출력되었습니다.
	}
    //=================================================================================================
}

static void
wait_for_children(int signo)
{
	Task *task;
	pid_t pid;

rewait:
	pid = waitpid(-1, NULL, WNOHANG);
	if (pid <= 0)
		return;

	task = lookup_task_by_pid(pid);
	if (!task)
	{
		MSG("unknown pid %d", pid);
		return;
	}

	if (0) MSG("program[%s] terminated\n", task->id);

	if (running && task->action == ACTION_RESPAWN)
		spawn_task(task);
	else
		task->pid = 0;

	/* some SIGCHLD signals is lost... */
	goto rewait;
}

static void
terminate_children(int signo)
{
	Task *task;

	if (1) MSG("terminated by SIGNAL(%d)\n", signo);

	running = 0;

	for (task = tasks; task != NULL; task = task->next)
		if (task->pid > 0)
		{
			if (0) MSG("kill program[%s] by SIGNAL(%d)\n", task->id, signo);
			kill(task->pid, signo);
		}

	exit(1);
}

int
main(int    argc,
	char **argv)
{//==============================================Edit Part===============================================
	struct sigaction sa;
	struct signalfd_siginfo fdsi;//fdsi라는 signalfd용 구조체를 생성합니다.
	int terminated;
	srand((unsigned int)time(NULL));//rand()를 위한 시간 씨드 생성합니다.
	//==================================================================================================
	if (argc <= 1)
	{
		MSG("usage: %s config-file\n", argv[0]);
		return -1;
	}

	if (read_config(argv[1]))
	{
		MSG("failed to load config file '%s': %s\n", argv[1], STRERROR);
		return -1;
	}

	running = 1;


	/* SIGCHLD */
	/*sigemptyset (&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = wait_for_children;
	if (sigaction (SIGCHLD, &sa, NULL))
	  MSG ("failed to register signal handler for SIGINT\n");*/
	//==========================================Edit Part===================================================
	sigset_t mymask;
	int mysfd;
	ssize_t s;//signalfd에 필요한 변수들입니다.

	sigemptyset(&mymask);//mymask를 비웁니다.
	sigaddset(&mymask, SIGCHLD);//SIGCHLD 시그널을 처리하기 위해 추가하였습니다.
	sigaddset(&mymask, SIGQUIT);//SIGQUIT 시그널을 처리하기 위해 추가하였습니다.
	mysfd = signalfd(-1, &mymask, 0);//signalfd에 mask값을 입력합니다.
	if (mysfd == -1)//-1이 반환될 경우 오류메세지를 띄워줍니다.
		MSG("failed to signal handler SIGCHLD");
	if (sigprocmask(SIG_BLOCK, &mymask, NULL) == -1)//sigprocmask 시스템콜을 호출합니다.
	{
		MSG("error");
	}//====================================================================================================
	/* SIGINT */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = terminate_children;
	if (sigaction(SIGINT, &sa, NULL))
		MSG("failed to register signal handler for SIGINT\n");

	/* SIGTERM */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = terminate_children;
	if (sigaction(SIGTERM, &sa, NULL))
		MSG("failed to register signal handler for SIGINT\n");

	spawn_tasks();

	terminated = 0;
	while (!terminated)
	{
		Task *task;

		terminated = 1;
		for (task = tasks; task != NULL; task = task->next)
			if (task->pid > 0)
			{
				terminated = 0;
				break;
			}
		//===============================Edit Part===========================================================
		s = read(mysfd, &fdsi, sizeof(struct signalfd_siginfo));//signalfd의 구조체를 읽어옵니다.
		if (s != sizeof(struct signalfd_siginfo))//제대로 불러오지 못했을 경우 오류를 출력합니다.
			MSG("read error\n");
		if (fdsi.ssi_signo == SIGCHLD)//만약 입력받은 시그널이 SIGCHLD일 경우
		{
			wait_for_children(SIGCHLD);//동기방식으로 wait_for_children함수를 호출합니다.
			//printf("Got SIGCHLD\n");//시그널을 제대로 받아오는지 체크하기 위한 출력부입니다.
		}
		else if (fdsi.ssi_signo == SIGQUIT)//SIGQUIT 시그널이 올경우
		{
			exit(EXIT_SUCCESS);//종료시킵니다.
		}
		else {
			printf("Read Unexpected signal\n");//전혀 이상한 시그널을 받았을때의 예외처리입니다.
		}
		
		usleep(100000);
	}
	//=====================================================================================================
	return 0;
}
