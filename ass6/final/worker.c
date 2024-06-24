#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>

#define V(s) semop(s, &vop, 1)
#define P(s) semop(s, &pop, 1)

int main(int argc, char* argv[])
{
    int n = atoi(argv[1]);                                                                      // Reading the number of vertices
    int self = atoi(argv[2]);                                                                   // Reading the self number

    //Key copy
    key_t key_notify = ftok("graph.txt", 3);
    key_t key_sync = ftok("graph.txt", 4);
    key_t key_mtx = ftok("graph.txt", 5);

    int shmid_adj = shmget(ftok("graph.txt", 0), n * n * sizeof(int), 0777 | IPC_CREAT);
    int shmid_T = shmget(ftok("graph.txt", 1), n * sizeof(int), 0777 | IPC_CREAT);
    int shmid_idx = shmget(ftok("graph.txt", 2), sizeof(int), 0777 | IPC_CREAT);

    int *shm_adj = (int *)shmat(shmid_adj, 0, 0);
    int *shm_idx = (int *)shmat(shmid_idx, 0, 0);
    int *shm_T = (int *)shmat(shmid_T, 0, 0);

    struct sembuf pop, vop ;
    pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1 ;

    int semid_notify = semget(key_notify, 1, 0777 | IPC_CREAT);
    int semid_sync = semget(key_sync, n, 0777 | IPC_CREAT);
    int semid_mtx = semget(key_mtx, 1, 0777 | IPC_CREAT);

    for(int i = 0; i < n; i++)                                              // Wait on the incoming edges
    {
        if(shm_adj[i * n + self] == 1)
        {
            pop.sem_num = i;
            P(semid_sync);
        }
    }

    pop.sem_num = 0;                                                        // Wait on the mutex, so assigning the mutex index to 0
    vop.sem_num = 0;

    P(semid_mtx);                                                           //Entry section
    shm_T[*shm_idx] = self;                                                 //Critical section
    (*shm_idx)++;
    V(semid_mtx);                                                           //Exit section
    
    V(semid_notify);                                                        //Notify the boss that the worker has finished

    vop.sem_num = self;                                                     //Signal the outgoing edges, so assigning the semaphore index to self
    for(int i = 0; i < n; i++)
    {
        if(shm_adj[self * n + i] == 1)
        {
            V(semid_sync);
        }
    }

    shmdt(shm_adj);                                                         //Detaching the shared memory
    shmdt(shm_idx);
    shmdt(shm_T);
    return 0;
}