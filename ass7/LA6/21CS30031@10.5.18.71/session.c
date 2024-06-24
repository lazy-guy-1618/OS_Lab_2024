#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "event.h"

#define MAX_PATIENT_COUNT 25
#define MAX_SALES_REP_COUNT 3

// Function prototypes
void *assistant(void *arg);
void *doctor(void *arg);
void *patient(void *arg);
void *reporter(void *arg);
void *salesrep(void *arg);

typedef struct {
    int number;
    int accept_time;
    int duration;
} visitor_data;

int event_priority(char type) {
    switch(type) {
        case 'R': return 1;
        case 'P': return 2;
        case 'S': return 3;
        case 'D': return 4;
        default: return 5;
    }
}

// Global condition variable and mutex

pthread_mutex_t patient_mutex, reporter_mutex, sales_rep_mutex;
pthread_mutex_t new_visitor;
pthread_mutex_t mutex;
pthread_mutex_t time_mutex;
pthread_mutex_t done_mutex;
pthread_cond_t cond_done;
pthread_cond_t cond_doctor_done;
pthread_cond_t cond_reporter, cond_patient, cond_sales_rep;
int reporter_done = 0, patient_done = 0, sales_rep_done = 0;
int Doc_time = 0;
int all_done = 0;
int doc_available = 1;
int visitor_done = 1;
int done_counter = 0;
int reporter_wait_count = 0;
int patient_wait_count = 0;
int sales_rep_wait_count = 0;

int eventcmpWrapper(const void * a, const void * b) {
   return eventcmp(*(event*)a, *(event*)b);
}

int main() {
    // Initialize event queue with arrival records
    eventQ E = initEQ("arrival.txt");

    // Sort the event queue
    qsort(E.Q, E.n, sizeof(event), eventcmpWrapper);

    // Create assistant thread
    pthread_t assistant_thread;
    pthread_create(&assistant_thread, NULL, assistant, (void *)&E);

    // Wait for assistant thread to finish
    pthread_join(assistant_thread, NULL);

    return 0;
}

void *assistant(void *arg) {
    eventQ *event_queue = (eventQ *)arg;
    int patient_count = 0;
    int sales_rep_count = 0;
    int reporter_count = 0;
    int left_reporter = 0;
    int arrival_time;
    int empty = 1;
    int D_created = 0;

    // Count the number of events where e.type == 'R'
    for (int i = 0; i < event_queue->n; i++) {
        if (event_queue->Q[i].type == 'R') {
            left_reporter++;
        }
        if (event_queue->Q[i].type == 'E') {
            break;
        }
    }


    // Create the doctor thread
    pthread_t doctor_thread;
    pthread_create(&doctor_thread, NULL, doctor, (void *)&Doc_time);

    // Initialize the mutex and condition variables
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&done_mutex, NULL);
    pthread_cond_init(&cond_done, NULL);
    pthread_cond_init(&cond_doctor_done, NULL);
    pthread_mutex_init(&time_mutex, NULL);
    pthread_mutex_init(&patient_mutex, NULL);
    pthread_mutex_init(&reporter_mutex, NULL);
    pthread_mutex_init(&sales_rep_mutex, NULL);
    pthread_mutex_init(&new_visitor, NULL);
    pthread_cond_init(&cond_reporter, NULL);
    pthread_cond_init(&cond_patient, NULL);
    pthread_cond_init(&cond_sales_rep, NULL);

    // While the event queue E is not empty
    while (!emptyQ(*event_queue)) {
        if(sales_rep_count >= 3 && patient_count >= 25 && left_reporter == 0){
            all_done = 1;
            if(doc_available) pthread_mutex_unlock(&new_visitor);
        }

        if(doc_available == 1 && visitor_done == 1 && D_created == 0 && all_done == 0){              // Creating D event
            if(!empty){
                event e;
                e.type = 'D';
                e.time = Doc_time;
                e.duration = 0;

                // Insert the event into the event_queue
                int i, p;
                event t;

                event_queue->Q[event_queue->n] = e;
                i = event_queue->n;
                while (1) {
                    if (i == 0) break;
                    p = (i - 1) / 2;
                    // Compare first on time of event and in case of tie, on type of event
                    if ((event_queue->Q[p].time < event_queue->Q[i].time) || 
                        (event_queue->Q[p].time == event_queue->Q[i].time && event_priority(event_queue->Q[p].type) < event_priority(event_queue->Q[i].type))) break;
                    t = event_queue->Q[i]; event_queue->Q[i] = event_queue->Q[p]; event_queue->Q[p] = t;
                    i = p;
                }
                ++event_queue->n;
                D_created = 1;
                continue;
            }
        }
        // Extract the next event e from the event queue E
        event e = nextevent(*event_queue);
        *event_queue = delevent(*event_queue);

        // Update the current time with the time stored in e
        pthread_mutex_lock(&time_mutex);
        arrival_time = e.time;
        pthread_mutex_unlock(&time_mutex);

        // If the event e is a new visitor
        if (e.type == 'R' || e.type == 'P' || e.type == 'S') {
            pthread_mutex_lock(&mutex);
            char* visitorType;
            int* visitorCount;
            int* visitorDone;
            pthread_cond_t* visitorCond;
            pthread_t visitor_thread;
            int maxCount;

            if (e.type == 'R') {
                visitorType = "Reporter";
                visitorCount = &reporter_count;
                visitorDone = &reporter_done;
                visitorCond = &cond_reporter;
                (*visitorCount)++;
                int hours = 9 + arrival_time / 60;
                int minutes = arrival_time % 60;

                if (minutes < 0) {
                    minutes += 60;
                    hours--;
                }
                printf("\t\t\t[%02d:%02dam] %s %d arrives\n", hours, minutes, visitorType, *visitorCount);

                visitor_data *data = malloc(sizeof(visitor_data));
                data->number = *visitorCount;
                data->duration = e.duration;

                // Lock the mutex before accessing Doc_time
                pthread_mutex_lock(&time_mutex);
                data->accept_time = Doc_time;
                pthread_mutex_unlock(&time_mutex);

                if(empty) empty = 0;
                left_reporter--;
                pthread_create(&visitor_thread, NULL, reporter, data);

            } else if (e.type == 'P') {
                visitorType = "Patient";
                visitorCount = &patient_count;
                visitorDone = &patient_done;
                visitorCond = &cond_patient;
                maxCount = MAX_PATIENT_COUNT;
                (*visitorCount)++;
                int hours = 9 + arrival_time / 60;
                int minutes = arrival_time % 60;

                if (minutes < 0) {
                    minutes += 60;
                    hours--;
                }
                if(*visitorCount <= maxCount){
                    printf("\t\t\t[%02d:%02dam] %s %d arrives\n", hours, minutes, visitorType, *visitorCount);
                    
                    visitor_data *data = malloc(sizeof(visitor_data));
                    data->number = *visitorCount;
                    data->duration = e.duration;

                    // Lock the mutex before accessing Doc_time
                    pthread_mutex_lock(&time_mutex);
                    data->accept_time = Doc_time;
                    pthread_mutex_unlock(&time_mutex);

                    if(empty) empty = 0;
                    pthread_create(&visitor_thread, NULL, patient, data);
                }
                else {
                    printf("\t\t\t[%02d:%02dam] %s %d arrives\n", hours, minutes, visitorType, *visitorCount);
                    printf("\t\t\t[%02d:%02dam] %s %d leaves (quota full)\n", hours, minutes, visitorType, *visitorCount);
                }
            } else if(e.type == 'S') {
                visitorType = "Sales representative";
                visitorCount = &sales_rep_count;
                visitorDone = &sales_rep_done;
                visitorCond = &cond_sales_rep;
                maxCount = MAX_SALES_REP_COUNT;
                (*visitorCount)++;
                int hours = 9 + arrival_time / 60;
                int minutes = arrival_time % 60;

                if (minutes < 0) {
                    minutes += 60;
                    hours--;
                }
                if(*visitorCount <= maxCount){
                    printf("\t\t\t[%02d:%02dam] %s %d arrives\n", hours, minutes, visitorType, *visitorCount);
                    
                    visitor_data *data = malloc(sizeof(visitor_data));
                    data->number = *visitorCount;
                    data->duration = e.duration;

                    // Lock the mutex before accessing Doc_time
                    pthread_mutex_lock(&time_mutex);
                    data->accept_time = Doc_time;
                    pthread_mutex_unlock(&time_mutex);

                    if(empty) empty = 0;
                    pthread_create(&visitor_thread, NULL, salesrep, data);
                }
                else {
                    printf("\t\t\t[%02d:%02dam] %s %d arrives\n", hours, minutes, visitorType, *visitorCount);
                    printf("\t\t\t[%02d:%02dam] %s %d leaves (quota full)\n", hours, minutes, visitorType, *visitorCount);
                }
            }

            pthread_mutex_unlock(&mutex);
        } else if (e.type == 'D') {
            pthread_mutex_lock(&mutex);

            pthread_mutex_lock(&time_mutex);
            Doc_time = e.time;
            pthread_mutex_unlock(&time_mutex);

            if(reporter_wait_count > 0 || patient_wait_count > 0 || sales_rep_wait_count > 0){
                pthread_mutex_unlock(&new_visitor);
                if(reporter_wait_count > 0){
                    pthread_cond_signal(&cond_reporter);
                } else if(patient_wait_count > 0){
                    pthread_cond_signal(&cond_patient);
                } else if(sales_rep_wait_count > 0){
                    pthread_cond_signal(&cond_sales_rep);
                }

                // For the doctor and visito both to finish
                while (1) {
                    pthread_cond_wait(&cond_done, &mutex);  // Wait for the doctor and the visitor to finish
                    pthread_mutex_lock(&done_mutex);
                    if (done_counter == 2) {
                        done_counter = 0;
                        pthread_mutex_unlock(&done_mutex);
                        break;
                    }
                    pthread_mutex_unlock(&done_mutex);
                }
            }
            else{
                empty = 1;
                continue;
            }
            D_created = 0;
            pthread_mutex_unlock(&mutex);
        }
    }

    pthread_join(doctor_thread, NULL);      // Wait for doctor thread to finish
    // Destroy the mutex and condition variables
    pthread_mutex_destroy(&patient_mutex);
    pthread_mutex_destroy(&reporter_mutex);
    pthread_mutex_destroy(&sales_rep_mutex);
    pthread_mutex_destroy(&new_visitor);
    pthread_cond_destroy(&cond_reporter);
    pthread_cond_destroy(&cond_patient);
    pthread_cond_destroy(&cond_sales_rep);

    pthread_exit(NULL);
}

void *doctor(void *arg) {
    while(1){
        if(all_done == 1){
            pthread_mutex_lock(&time_mutex);
            int hours = 9 + Doc_time / 60;
            int minutes = Doc_time % 60;

            if (minutes < 0) {
                minutes += 60;
                hours--;
            }
            printf("[%02d:%02dam] Doctor leaves\n", hours, minutes);
            pthread_mutex_unlock(&time_mutex);
            pthread_exit(NULL);
        }
        doc_available = 1;
        pthread_mutex_lock(&new_visitor);
        if(all_done == 1){
            pthread_mutex_unlock(&new_visitor);
            continue;
        }
        doc_available = 0;
        int hours = 9 + Doc_time / 60;
        int minutes = Doc_time % 60;

        if (minutes < 0) {
            minutes += 60;
            hours--;
        }
        printf("[%02d:%02dam] Doctor has next visitor\n", hours, minutes);
        pthread_mutex_unlock(&new_visitor);

        pthread_mutex_lock(&done_mutex);
        done_counter++;
        pthread_mutex_unlock(&done_mutex);

        pthread_cond_signal(&cond_doctor_done);  // Signal that the doctor is done
        pthread_cond_signal(&cond_done);
    }
}

void *patient(void *arg) {
    visitor_data *data = (visitor_data *)arg;
    int number = data->number;
    int accept_time = data->accept_time;
    int duration = data->duration;

    // Lock the mutex before waiting on the condition variable
    pthread_mutex_lock(&patient_mutex);

    patient_wait_count++;

    // Wait on the condition variable
    pthread_cond_wait(&cond_patient, &patient_mutex);

    // After being signaled and waking up, calculate the end time
    pthread_mutex_lock(&time_mutex);
    visitor_done = 0;

    accept_time = Doc_time;
    int End_time = accept_time + duration;

    int hours_accept = 9 + accept_time / 60;
    int hours_end = 9 + End_time / 60;
    int minutes_accept = accept_time % 60;
    int minutes_end = End_time % 60;

    if (minutes_accept < 0) {
        minutes_accept += 60;
        hours_accept--;
    }
    if(minutes_end < 0){
        minutes_end += 60;
        hours_end--;
    }
    pthread_cond_wait(&cond_doctor_done, &patient_mutex);  // Wait for the doctor to finish
    printf("[%02d:%02dam - %02d:%02dam] Patient %d is in doctor's chamber\n", hours_accept, minutes_accept, hours_end, minutes_end, number);
    Doc_time = End_time;

    visitor_done = 1;
    pthread_mutex_unlock(&time_mutex);

    pthread_mutex_lock(&done_mutex);
    done_counter++;
    pthread_mutex_unlock(&done_mutex);

    patient_wait_count--;
 
    pthread_cond_signal(&cond_done);  // Signal that the patient is done
    pthread_mutex_unlock(&patient_mutex);

    pthread_exit(NULL);
}

void *reporter(void *arg) {
    visitor_data *data = (visitor_data *)arg;
    int number = data->number;
    int accept_time = data->accept_time;
    int duration = data->duration;

    // Lock the mutex before waiting on the condition variable
    pthread_mutex_lock(&reporter_mutex);

    reporter_wait_count++;

    // Wait on the condition variable
    pthread_cond_wait(&cond_reporter, &reporter_mutex);

    // After being signaled and waking up, calculate the end time
    pthread_mutex_lock(&time_mutex);
    visitor_done = 0;

    accept_time = Doc_time;
    int End_time = accept_time + duration;

    int hours_accept = 9 + accept_time / 60;
    int hours_end = 9 + End_time / 60;
    int minutes_accept = accept_time % 60;
    int minutes_end = End_time % 60;

    if (minutes_accept < 0) {
        minutes_accept += 60;
        hours_accept--;
    }
    if(minutes_end < 0){
        minutes_end += 60;
        hours_end--;
    }
    pthread_cond_wait(&cond_doctor_done, &patient_mutex);  // Wait for the doctor to finish
    printf("[%02d:%02dam - %02d:%02dam] Reporter %d is in doctor's chamber\n", hours_accept, minutes_accept, hours_end, minutes_end, number);

    Doc_time = End_time;

    visitor_done = 1;
    pthread_mutex_unlock(&time_mutex);

    pthread_mutex_lock(&done_mutex);
    done_counter++;
    pthread_mutex_unlock(&done_mutex);
 
    reporter_wait_count--;
    pthread_cond_signal(&cond_done);  // Signal that the patient is done
    pthread_mutex_unlock(&reporter_mutex);

    pthread_exit(NULL);
}

void *salesrep(void *arg) {
    visitor_data *data = (visitor_data *)arg;
    int number = data->number;
    int accept_time = data->accept_time;
    int duration = data->duration;

    // Lock the mutex before waiting on the condition variable
    pthread_mutex_lock(&sales_rep_mutex);

    sales_rep_wait_count++;

    // Wait on the condition variable
    pthread_cond_wait(&cond_sales_rep, &sales_rep_mutex);

    // After being signaled and waking up, calculate the end time
    pthread_mutex_lock(&time_mutex);
    visitor_done = 0;

    int End_time = accept_time + duration;

    int hours_accept = 9 + accept_time / 60;
    int hours_end = 9 + End_time / 60;
    int minutes_accept = accept_time % 60;
    int minutes_end = End_time % 60;

    if (minutes_accept < 0) {
        minutes_accept += 60;
        hours_accept--;
    }
    if(minutes_end < 0){
        minutes_end += 60;
        hours_end--;
    }
    pthread_cond_wait(&cond_doctor_done, &patient_mutex);  // Wait for the doctor to finish
    printf("[%02d:%02dam - %02d:%02dam] Sales representative %d is in doctor's chamber\n", hours_accept, minutes_accept, hours_end, minutes_end, number);

    Doc_time = End_time;

    visitor_done = 1;
    pthread_mutex_unlock(&time_mutex);  

    pthread_mutex_lock(&done_mutex);
    done_counter++;
    pthread_mutex_unlock(&done_mutex);
 
    sales_rep_wait_count--;
    pthread_cond_signal(&cond_done);  // Signal that the patient is done
    pthread_mutex_unlock(&sales_rep_mutex);

    pthread_exit(NULL);
}
