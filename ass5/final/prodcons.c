#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>

int main(){

    int shmid;
    int *M;                         // Pointer to the base of shared memory
    int n, t;

    printf("n =  ");
    scanf("%d", &n);
    printf("t =  ");
    scanf("%d", &t);
    shmid = shmget(IPC_PRIVATE, 2*sizeof(int), IPC_CREAT|0666);
    M = (int *)shmat(shmid, NULL, 0);
    M[0] = M[1] = 0;
    int sum = 0;
    int count_items = 0;
    for(int  i = 0; i<n ; ++i){
        if(fork() == 0){  
            int *a;
            a = (int *)shmat(shmid, NULL, 0);                                      // Child process consumer loop
            while(1){
                // printf("%d works", index);
                while(a[0] == 0) {}
                if(a[0] == -1) break;
                if(a[0] == i+1){
                    printf("Consumer %d is alive\n", i+1);
                    #ifdef VERBOSE
                    printf("\t\t\tConsumer %d reads %d\n", i+1, a[1]);
                    #endif
                    sum += a[1];
                    count_items++;
                    a[0] = 0;
                }
            }
            if(count_items > 0){
                printf("\t\t\tConsumer %d has read %d items : Checksum = %d\n", i+1, count_items, sum);
            }
            shmdt(a);
            exit(0);
        }
    }

    // Parent process producer loop
    printf("Producer is alive\n");
    int *checksum = (int *)malloc(n*sizeof(int));
    int *items = (int *)malloc(n*sizeof(int));
    for(int i = 0; i<t; ++i) {checksum[i] = 0; items[i] = 0;}
    for(int i = 0; i<t; ++i){
        while(M[0] != 0) {}
        M[0] = rand()%n + 1;
        #ifdef SLEEP
        usleep(5);
        #endif
        M[1] = rand()%1000;
        #ifdef VERBOSE
        printf("Producer produces %d for consumer %d\n", M[1], M[0]);
        #endif
        checksum[M[0]-1] += M[1];
        items[M[0]-1]++;
    }
    while(M[0] != 0) {}
    M[0] = -1;
    for(int i = 0; i<n; ++i) wait(NULL);
    shmdt(M);
    shmctl(shmid, IPC_RMID, NULL);
    printf("Producer has produced %d items\n", t);
    for(int i = 0; i<n; ++i) if(items[i]>0) printf("%d items produced for consumer %d : Checksum = %d\n", items[i], i+1, checksum[i]);

    return 0;
}