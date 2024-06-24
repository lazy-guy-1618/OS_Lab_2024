#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

pid_t pid;                                                  // fflush(stdout) is used to flush the output buffer so that the output is printed immediately. This is done because the parent and child processes are running concurrently and the output of the child process might be printed before the output of the parent process. This is done to avoid that.
char input[2];

struct process_table
{
    int index;
    pid_t pid;
    pid_t pgid;
    char status[20];
    char *name;
};

struct process_table PT[11];
int i = 0;
int j = 0;

void abc(int sig)
{
    if (input[0] == 'r' || input[0] == 'c')                 // If the input is "r" or "c", then the signal is to be handled by the parent process and changes to be made in the process table else "ignore" the signal
    {
        if (sig == SIGINT)
        {
            if (pid)
            {
                kill(pid, SIGKILL);
                if (input[0] == 'r')
                    strcpy(PT[i - 1].status, "TERMINATED");
                else
                    strcpy(PT[j].status, "TERMINATED");
                printf("\nmgr> ");
                fflush(stdout);
            }
            else
            {
                signal(SIGINT, SIG_DFL);
            }
        }

        else if (sig == SIGTSTP)
        {
            if (pid)
            {
                if (input[0] == 'r')
                {
                    strcpy(PT[i - 1].status, "SUSPENDED");
                }
                else if (input[0] == 'c')
                {
                    strcpy(PT[j].status, "SUSPENDED");
                }
                kill(pid, SIGTSTP);
                // Uncomment the following lines to print the pgid of the child process and verify that the pgid of the child has indeed been changed
                // printf("Child process pgid: %d\n", getpgid(pid));
                // fflush(stdout);
            }
            else
            {
                signal(SIGTSTP, SIG_DFL);
            }
        }

        else if (sig == SIGCHLD)
        {
            printf("\nmgr> ");
            fflush(stdout);
        }
        input[0] = '0';                                         // Reset the input
    }
    else
    {
        if (sig == SIGINT || sig == SIGTSTP)
        {
            printf("\nmgr> ");
            fflush(stdout);
        }
    }
}

int main()
{
    int target;
    int suspended = 0;
    char argg[2];
    char *temp = (char *)malloc(10);

    // Initialize the process table
    PT[0].index = 0;
    PT[0].pid = getpid();
    PT[0].pgid = getpgid(PT[0].pid);
    strcpy(PT[0].status, "SELF");
    PT[0].name = "mgr";
    i++;
    while (1)
    {
        signal(SIGTSTP, abc);
        signal(SIGINT, abc);
        signal(SIGCHLD, abc);
        printf("mgr> ");
        fflush(stdout);
        scanf("%s", input);
        switch (input[0])
        {
        case 'c':
            suspended = 0;
            printf("Suspended jobs: ");
            for (int k = 0; k < i; ++k)
            {
                if (strcmp(PT[k].status, "SUSPENDED") == 0)
                {
                    suspended = 1;
                    printf("%d, ", PT[k].index);
                }
            }
            if (!suspended)
            {
                printf("No process suspended to continue !\n");
                break;
            }
            printf("\b\t(Pick one): ");
            scanf("%d", &target);
            if (target >= i || strcmp(PT[target].status, "SUSPENDED") != 0)
            {
                printf("Invalid process index !\n");
                break;
            }
            pid = PT[target].pid;
            j = target;
            kill(PT[target].pid, SIGCONT);
            strcpy(PT[target].status, "FINISHED");

            break;

        case 'h':

            printf("Command\t:\tAction\n");
            printf("c\t:\tContinue a suspended job\n");
            printf("h\t:\tPrint this help message\n");
            printf("k\t:\tKill a suspended job\n");
            printf("p\t:\tPrint the process table\n");
            printf("q\t:\tQuit\n");
            printf("r\t:\tRun a new job\n");

            break;

        case 'k':
            suspended = 0;
            printf("Suspended jobs: ");
            for (int k = 0; k < i; ++k)
            {
                if (strcmp(PT[k].status, "SUSPENDED") == 0)
                {
                    suspended = 1;
                    printf("%d, ", PT[k].index);
                }
            }
            if (!suspended)
            {
                printf("No process suspended to kill !\n");
                break;
            }
            printf("\b\t(Pick one): ");
            scanf("%d", &target);
            if (target >= i || strcmp(PT[target].status, "SUSPENDED") != 0)
            {
                printf("Invalid process index !\n");
                break;
            }
            kill(PT[target].pid, SIGKILL);
            strcpy(PT[target].status, "TERMINATED");

            break;

        case 'p':
            printf("Index\tPID\tPGID\tStatus\t\tName\n");
            printf("%d\t%d\t%d\t%s\t\t%s\n", PT[0].index, PT[0].pid, PT[0].pgid, PT[0].status, PT[0].name);
            for (int k = 1; k < i; ++k)
            {
                printf("%d\t%d\t%d\t%s\t%s\n", PT[k].index, PT[k].pid, PT[k].pgid, PT[k].status, PT[k].name);
            }
            break;

        case 'q':
            exit(0);
            break;

        case 'r':
            if (i == 11)
            {   // If the process table is full, print the message and exit
                printf("Process table is full. Quitting ...\n");
                exit(0);
            }
            argg[0] = 'A' + rand() % 26;
            argg[1] = '\0';
            pid = fork();
            if (pid)
            {
                sprintf(temp, "job %c", argg[0]);
                PT[i].index = i;
                PT[i].pid = pid;
                PT[i].pgid = pid; // Technically, pgid(pid) should have been used in the RHS, however, since the child process and the parent process are running concurrently, it might happen that setpgid() in the child body is executed after this line is executed, which would result in the the old pgid for the child to be displayed instead of the new one by setpgid().
                                  // In order to verify that child process has indeed got a new pgid, please uncomment the lines in the abc() function.
                strcpy(PT[i].status, "FINISHED");
                PT[i].name = strdup(temp);
                i++;
                waitpid(pid, NULL, WNOHANG | WUNTRACED);        // WNOHANG is used to return immediately if no child has exited. WUNTRACED is used to return if a child has stopped . This is useful to check whether it has been stopped because of a job control signal.
            }
            else
            {
                setpgid(0, 0);
                execlp("./job", "./job", argg, NULL);
                exit(0);
            }
            break;

        default:
            printf("Invalid command, please type \"h\" for help\n");
            break;
        }
    }
    return 0;
}