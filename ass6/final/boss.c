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

int main()
{                                                           // Opening the file
    FILE *file = fopen("graph.txt", "r");
    if (file == NULL)
    {
        printf("Cannot open file.\n");
        return 1;
    }

    int n;
    fscanf(file, "%d", &n);                             // Reading the number of vertices

    key_t key_notify = ftok("graph.txt", 3);
    key_t key_sync = ftok("graph.txt", 4);
    key_t key_mtx = ftok("graph.txt", 5);

    // Shared memory
    int shmid_adj = shmget(ftok("graph.txt", 0), n * n * sizeof(int), 0777 | IPC_CREAT);
    int shmid_T = shmget(ftok("graph.txt", 1), n * sizeof(int), 0777 | IPC_CREAT);
    int shmid_idx = shmget(ftok("graph.txt", 2), sizeof(int), 0777 | IPC_CREAT);

    int *shm_adj = (int *)shmat(shmid_adj, 0, 0);
    int *shm_idx = (int *)shmat(shmid_idx, 0, 0);
    *shm_idx = 0;
    int *shm_T = (int *)shmat(shmid_T, 0, 0);
    if (shm_adj == (int *)-1)
    {                                                   // Error handling
        printf("shmat failed\n");
        return 1;
    }

    for (int i = 0; i < n; i++)                         // Reading the adjacency matrix
    {
        for (int j = 0; j < n; j++)
        {
            fscanf(file, "%d", &shm_adj[i * n + j]);
        }
    }

    fclose(file);

    struct sembuf pop, vop;                             // Semaphore operations
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;                                 // P operation             
    vop.sem_op = 1;                                 // V operation

    // Semaphore creation
    int semid_notify = semget(key_notify, 1, 0777 | IPC_CREAT);
    int semid_sync = semget(key_sync, n, 0777 | IPC_CREAT);
    int semid_mtx = semget(key_mtx, 1, 0777 | IPC_CREAT);

    // Semaphore initialization
    semctl(semid_notify, 0, SETVAL, 0);
    for (int i = 0; i < n; i++)
    {
        semctl(semid_sync, i, SETVAL, 0);
    }
    semctl(semid_mtx, 0, SETVAL, 1);

    printf("+++ Boss: Setup done...\n");

    for (int i = 0; i < n; ++i)                                             // Wait for all workers to finish
    {
        P(semid_notify);
    }

    int valid = 1;
    for (int j = 0; j < n; j++)                                             // for each vertex in T
    {
        int self = shm_T[j];
        for (int i = 0; i < n; i++)                                         // for each predecessor vertex in V 
        {
            if (shm_adj[i * n + self] == 1)
            {
                int found = 0;
                for (int k = 0; k < j; k++)                                 // for each vertex in T
                {
                    if (shm_T[k] == i)
                    {
                        found = 1;
                        break;
                    }
                }
                if (!found)                                                 // if the predecessor vertex is not in T
                {
                    valid = 0;
                    break;
                }
            }
        }
        if (!valid)
        {
            break;
        }
    }
    if (valid)                                                          // if the topological sorting is valid
    {
        printf("+++ Topological sorting of the vertices: \n");
        for (int i = 0; i < n; i++)
        {
            printf("%d\t", shm_T[i]);
        }
        printf("\n+++ Boss: Well done, my team...\n");
    }
    else
    {
        printf("+++ Boss: You have failed me, my team...\n");
    }

    shmdt(shm_adj);
    shmdt(shm_idx);
    shmdt(shm_T);
    shmctl(shmid_adj, IPC_RMID, 0);
    shmctl(shmid_idx, IPC_RMID, 0);
    shmctl(shmid_T, IPC_RMID, 0);
    semctl(semid_notify, 0, IPC_RMID, 0);
    semctl(semid_sync, 0, IPC_RMID, 0);
    semctl(semid_mtx, 0, IPC_RMID, 0);
    return 0;
}