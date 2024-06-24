#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

struct msg1_buffer {
    long msg_type;
    struct msg1 {
        int pid;
    } msg;
};

struct msg2_buffer {
    long msg_type;
    struct msg2 {
        int type_of_msg;
    } msg;
};

int main(int argc, char *argv[]) {
    int mq1 = atoi(argv[1]);
    int mq2 = atoi(argv[2]);
    int k = atoi(argv[3]);

    printf("Scheduler started with k = %d\n", k);

    struct msg1_buffer msg1;
    struct msg2_buffer msg2;

    int count = 0;
    while(1) {
        if (k == 0) {
            msg1.msg_type = 2;
            msgsnd(mq1, &msg1, sizeof(msg1.msg), 0);
        }
        msgrcv(mq1, &msg1, sizeof(msg1.msg), 0, 0);    // Current process in the ready queue

        int pid = msg1.msg.pid;                         // pid of the current process
        usleep(250000);
        kill(pid, SIGCONT);

        msgrcv(mq2, &msg2, sizeof(msg2.msg), 0, 0);     // Status of the current process
        printf("Scheduler received message from MMU for %d\n", ++count);
        if (msg2.msg.type_of_msg == 1) {
            // PAGE FAULT HANDLED
            msg1.msg_type = 1;
            msg1.msg.pid = pid;
            msgsnd(mq1, &msg1, sizeof(msg1.msg), 0);
        }
        else {
            // PROCESS TERMINATED
            k--;
        }
    }
    return 0;
}