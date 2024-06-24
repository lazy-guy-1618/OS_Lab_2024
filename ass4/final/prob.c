#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

pid_t C_pid, S_pid, E_pid, pid;

int main(int argc, char *argv[]){
    int pfd1[2];
    pipe(pfd1);
    int pfd2[2];
    pipe(pfd2);
    S_pid = getpid();
    if(fork()){                     // S

        if(fork()){                 // C


        }
        else{                       // E

        }

    }


    return 0;
}