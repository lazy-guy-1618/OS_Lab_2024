#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define p 0.05              // Probability of illegal page reference

struct page_table {
    int frame_number;
    bool valid_bit;
};

int main() {
    srand(time(NULL));

    int k, m, f;
    printf("Enter number of processes: ");
    scanf("%d", &k);
    printf("Enter virtual space size (Max number of pages per process): ");
    scanf("%d", &m);
    printf("Enter number of frames: ");
    scanf("%d", &f);

    // Create Data Structures
    int shm1 = shmget(IPC_PRIVATE, k*m*(sizeof(struct page_table)), IPC_CREAT | 0666);      // Shared memory for page table. <frame_number, valid_bit>
    int shm2 = shmget(IPC_PRIVATE, (f+1)*sizeof(int), IPC_CREAT | 0666);         // Shared memory for free frame list. First element is the number of free frames. Always pick the last frame.
    int shm3 = shmget(IPC_PRIVATE, k*sizeof(int), IPC_CREAT | 0666);               // Shared memory for page number mapping
    int mq1 = msgget(IPC_PRIVATE, IPC_CREAT | 0666);                         // Message queue for ready queue
    int mq2 = msgget(IPC_PRIVATE, IPC_CREAT | 0666);                         // Message queue for scheduler-mmu communication
    int mq3 = msgget(IPC_PRIVATE, IPC_CREAT | 0666);                         // Message queue for mmu-process communication

    char mq1_str[10], mq2_str[10], mq3_str[10], shm1_str[10], shm2_str[10], shm3_str[10], k_str[10], m_str[10];
    sprintf(mq1_str, "%d", mq1);
    sprintf(mq2_str, "%d", mq2);
    sprintf(mq3_str, "%d", mq3);
    sprintf(shm1_str, "%d", shm1);
    sprintf(shm2_str, "%d", shm2);
    sprintf(shm3_str, "%d", shm3);
    sprintf(k_str, "%d", k);
    sprintf(m_str, "%d", m);

    // Initialize Data Structures
    struct page_table * page_table = (struct page_table *) shmat(shm1, NULL, 0);
    if (page_table == (void *) -1) {
        perror("shmat");
        // Handle the error...
        return 1;
    }

    for (int i = 0; i < k*m; i++) {
        page_table[i].frame_number = -1;
        page_table[i].valid_bit = false;
    }
    int * free_frame_list = (int *) shmat(shm2, NULL, 0);
    free_frame_list[0] = f;
    for (int i = 1; i <= f; i++) {
        free_frame_list[i] = i-1;
    }
    int * page_number_mapping = (int *) shmat(shm3, NULL, 0);

    // Pages per process and frame allocation
    int total_pages = 0;
    for (int i = 0; i < k; i++) {
        page_number_mapping[i] = rand()%m + 1;
        total_pages += page_number_mapping[i];
    }
    for (int i = 0; i < k; i++) {
        int num_frames = (int)((float)page_number_mapping[i]/total_pages*f);
        for (int j = 0; j < num_frames; j++) {
            page_table[i*m+j].frame_number = free_frame_list[free_frame_list[0]];         
            free_frame_list[0]--;
            page_table[i*m+j].valid_bit = true;
        }
    }

    printf("initialization done\n");
    fflush(stdout);

    
    // Create scheduler
    int scheduler_pid = fork();
    if (scheduler_pid == 0) {
        char *args[] = {"./scheduler", mq1_str, mq2_str, k_str, NULL};
        execvp(args[0], args);
    }

    // Create mmu in xterm
    int mmu_pid = fork();
    if (mmu_pid == 0) {
        char *args[] = {"xterm", "-T", "MMU", "-e", "./mmu", mq2_str, mq3_str, shm1_str, shm2_str, shm3_str , m_str, k_str, NULL};
        execvp(args[0], args);
    }

    // Create processes
    for (int i = 0; i < k; i++) {
        int x = 2*page_number_mapping[i] + rand()%(8*page_number_mapping[i] + 1);       // Length of reference string
        int ref_string[x+1];
        int num_digits = 0;
        for (int j = 0; j < x; j++) {
            if ((float)rand() / (float)RAND_MAX < p) ref_string[j] = page_number_mapping[i] + rand()%(f-page_number_mapping[i] + 1);        // Illegal page reference
            else ref_string[j] = rand()%page_number_mapping[i];        // Legal page reference
            num_digits += snprintf(NULL, 0, "%d", ref_string[j]);
        }
        ref_string[x] = -9;
        num_digits += snprintf(NULL, 0, "%d", ref_string[x]);
        char ref_string_str[num_digits+x+1];        // Print numbers with spaces
        ref_string_str[0] = '\0';
        for (int j = 0; j <= x; j++) {
            char temp[10];
            sprintf(temp, "%d", ref_string[j]);
            strcat(ref_string_str, temp);
            strcat(ref_string_str, " ");
        }

        int process_pid = fork();
        if (process_pid == 0) {
            char i_str[10];
            sprintf(i_str, "%d", i);
            printf("Process %d created with reference string %s\n", i, ref_string_str);
            char *args[] = {"./process", mq1_str, mq3_str, ref_string_str, i_str, NULL};
            execvp(args[0], args);
        }

        usleep(250000);        // Sleep for 250 ms
    }

    msgrcv(mq1, NULL, 0, 2, MSG_NOERROR);        // Wait for all processes to finish. Message type = 2

    usleep(1000);      // Giving time to mmu to cleanup
    // Terminating scheduler and mmu
    kill(scheduler_pid, SIGKILL);
    kill(mmu_pid, SIGKILL);

    // Detach and remove shared memory
    shmdt(page_table);
    shmdt(free_frame_list);
    shmdt(page_number_mapping);
    shmctl(shm1, IPC_RMID, NULL);
    shmctl(shm2, IPC_RMID, NULL);
    shmctl(shm3, IPC_RMID, NULL);

    // Remove message queues
    msgctl(mq1, IPC_RMID, NULL);
    msgctl(mq2, IPC_RMID, NULL);
    msgctl(mq3, IPC_RMID, NULL);
}