#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>

struct page_table {
    int frame_number;
    bool valid_bit;
};

// For mq3
struct msg3_buffer {
    long msg_type;
    struct msg3 {
        int pg_num;
        int index;
    }msg;
}; 

// For mq2
struct msg2_buffer {
    long msg_type;
    struct msg2 {
        int type_of_msg;
    } msg;
};


void PageFaultHandler(struct page_table * page_table, int * free_frame_list, int page_number, int process_idx, int m, int global_timestamp, int * last_used){
    int page_table_idx = process_idx * m + page_number;
    if(free_frame_list[0] > 0){
        // Use the last available frame to insert in to the page table
        page_table[page_table_idx].frame_number = free_frame_list[free_frame_list[0]--];
        page_table[page_table_idx].valid_bit = true;
        last_used[page_table_idx] = global_timestamp;
    }
    else{
        // Use local replacement using LRU
        int victim_page_idx = process_idx * m;
        int min_timestamp = global_timestamp;
        for(int i = 0; i<m; ++i){
            if(last_used[process_idx*m + i] < min_timestamp && page_table[process_idx*m + i].valid_bit == true){
                min_timestamp = last_used[process_idx*m + i];
                victim_page_idx = process_idx*m + i;
            }
        }
        page_table[victim_page_idx].valid_bit = false;
        last_used[victim_page_idx] = -1;
        page_table[page_table_idx].frame_number = page_table[victim_page_idx].frame_number;
        page_table[page_table_idx].valid_bit = true;
        last_used[page_table_idx] = global_timestamp;
    }

}

int main(int argc, char *argv[]) {
    if (argc != 8) {
        printf("Wrong number of arguments\n");
        exit(1);
    }

    // Convert command line arguments
    int mq2 = atoi(argv[1]);
    int mq3 = atoi(argv[2]);
    int shm1 = atoi(argv[3]);
    int shm2 = atoi(argv[4]);
    int shm3 = atoi(argv[5]);
    int m = atoi(argv[6]);
    int k = atoi(argv[7]);

    // Attach to shared memory
    struct page_table * page_table = (struct page_table *) shmat(shm1, NULL, 0);
    int * free_frame_list = (int *) shmat(shm2, NULL, 0);
    int * page_number_mapping = (int *) shmat(shm3, NULL, 0);

    // Global timestamp
    int global_timestamp = 0;

    int * last_used = (int *) malloc(k*m * sizeof(int));            // For LRU
    for(int i = 0; i<k*m; i++) last_used[i] = -1;                   // Initialize with -1 (Never used)

    // Output file
    FILE *output_file = fopen("result.txt", "w");

    int *page_fault_count = calloc(k, sizeof(int)); // Array to keep track of page faults for each process
    int *invalid_page_ref_count = calloc(k, sizeof(int)); // Array to keep track of invalid page references for each process
    int t = k;              // Copy of k for later use

    while(k>0) {
        // Wait for a page number from any process
        struct msg3_buffer demand;
        struct msg3_buffer supply;

        supply.msg_type = 2;

        struct msg2_buffer msg2;
        msg2.msg_type = 1;

        msgrcv(mq3, &demand, sizeof(demand.msg), 1, 0); // Get the page number from the process

        global_timestamp++; // Increment global timestamp

        int page_number = demand.msg.pg_num;
        int process_idx = demand.msg.index;
        int m_process = page_number_mapping[process_idx]; // Number of pages for the process

        if (page_number == -9) {
            // Process has finished
            k--;
            
            // Free the allocated frames back to free frames list
            int page_idx_base = process_idx * m;
            for(int i = 0; i<m_process; ++i){
                if(page_table[page_idx_base + i].frame_number != -1 && page_table[page_idx_base + i].valid_bit == true){
                    free_frame_list[++free_frame_list[0]] = page_table[page_idx_base + i].frame_number;
                    page_table[page_idx_base + i].frame_number = -1;
                    page_table[page_idx_base + i].valid_bit = false;
                }
            }

            // Send Type II message to Scheduler
            msg2.msg.type_of_msg = 2;
            msgsnd(mq2, &msg2, sizeof(msg2.msg), 0);
        } else {
            if(page_number >= m_process) {
                // Invalid page number
                printf("TRYING TO ACCESS INVALID PAGE REFERENCE\n");
                fflush(stdout);
                printf("Invalid page reference - (p%d,x%d)\n", process_idx, page_number);
                fflush(stdout);

                fprintf(output_file, "Invalid page reference - (p%d,x%d)\n", process_idx, page_number);
                invalid_page_ref_count[process_idx]++;

                // Process was terminated
                k--;
            
                // Free the allocated frames back to free frames list
                int page_idx_base = process_idx * m;
                for(int i = 0; i<m_process; ++i){
                    if(page_table[page_idx_base + i].frame_number != -1 && page_table[page_idx_base + i].valid_bit == true){
                        free_frame_list[++free_frame_list[0]] = page_table[page_idx_base + i].frame_number;
                        page_table[page_idx_base + i].frame_number = -1;
                        page_table[page_idx_base + i].valid_bit = false;
                    }
                }

                supply.msg.pg_num = -2;
                supply.msg.index = process_idx;
                msgsnd(mq3, &supply, sizeof(supply.msg), 0);

                msg2.msg.type_of_msg = 2;
                msgsnd(mq2, &msg2, sizeof(msg2.msg), 0);
            } else {
                int page_table_idx = process_idx * m + page_number;
                if(page_table[page_table_idx].valid_bit == false) {
                    // Page is not valid
                    printf("Page fault - (p%d,x%d)\n", process_idx, page_number);
                    fprintf(output_file, "Page fault - (p%d,x%d)\n", process_idx, page_number);
                    page_fault_count[process_idx]++;

                    PageFaultHandler(page_table, free_frame_list, page_number, process_idx, m, global_timestamp, last_used);
                    supply.msg.pg_num = -1;
                    supply.msg.index = process_idx;
                    msg2.msg.type_of_msg = 1;
                    msgsnd(mq3, &supply, sizeof(supply.msg), 0);
                    msgsnd(mq2, &msg2, sizeof(msg2.msg), 0);
                } else {
                    // Page is in memory
                    last_used[page_table_idx] = global_timestamp; // Update the timestamp
                    supply.msg.pg_num = page_table[page_table_idx].frame_number;
                    supply.msg.index = process_idx;
                    msgsnd(mq3, &supply, sizeof(supply.msg), 0);
                }
            }
            printf("Global ordering - (t%d,p%d,x%d)\n", global_timestamp, process_idx, page_number);
            fprintf(output_file, "Global ordering - (t%d,p%d,x%d)\n", global_timestamp, process_idx, page_number);
        }
    }

    // Print and write total counts
    for(int i = 0; i < t; i++) {
        printf("Total page faults for process p%d: %d\n", i, page_fault_count[i]);
        printf("Total invalid page references for process p%d: %d\n", i, invalid_page_ref_count[i]);
        fprintf(output_file, "Total page faults for process p%d: %d\n", i, page_fault_count[i]);
        fprintf(output_file, "Total invalid page references for process p%d: %d\n", i, invalid_page_ref_count[i]);
    }

    fclose(output_file);
    free(page_fault_count);
    free(invalid_page_ref_count);

    // Cleanup before exit
    shmdt(page_table);
    shmdt(free_frame_list);

    return 0;
}

