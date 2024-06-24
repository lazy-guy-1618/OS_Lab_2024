#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
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
        case 'D': return 1;
        case 'R': return 2;
        case 'P': return 3;
        case 'S': return 4;
        default: return 5;
    }
}

// Global condition variable and mutex

sem_t sem;

pthread_barrier_t barrier;

pthread_mutex_t patient_mutex, reporter_mutex, sales_rep_mutex;
pthread_mutex_t dummy;
pthread_mutex_t mutex;
pthread_mutex_t time_mutex;
pthread_mutex_t done_mutex;
pthread_cond_t cond_done;
pthread_cond_t cond_doctor_done;
pthread_cond_t cond_reporter, cond_patient, cond_sales_rep;
pthread_cond_t new_visitor;
int reporter_done = 0, patient_done = 0, sales_rep_done = 0;
int Doc_time = 0;
int all_done = 0;
int doc_available = 1;
int visitor_done = 1;
int done_counter = 0;
int reporter_wait_count = 0;
int patient_wait_count = 0;
int sales_rep_wait_count = 0;
int done_doctor = 0;
int reporter_ready = 0;
int patient_ready = 0;
int sales_rep_ready = 0;

int eventcmpWrapper(const void * a, const void * b) {
   return eventcmp(*(event*)a, *(event*)b);
}

int main() {
    // Initialize event queue with arrival records
    eventQ E = initEQ("arrival.txt");

    // Sort the event queue
    qsort(E.Q, E.n, sizeof(event), eventcmpWrapper);

    if (sem_init(&sem, 0, 0) != 0) {
        fprintf(stderr, "Could not initialize semaphore.\n");
        return 1;
    }

    // Create assistant thread
    pthread_t assistant_thread;
    pthread_create(&assistant_thread, NULL, assistant, (void *)&E);

    // Wait for assistant thread to finish
    pthread_join(assistant_thread, NULL);
    sem_destroy(&sem);

    return 0;
}

void *assistant(void *arg) {
    eventQ *event_queue = (eventQ *)arg;
    int patient_count = 0;
    int sales_rep_count = 0;
    int reporter_count = 0;
    int left_reporter = 0;
    int arrival_time;
    int empty = 0;
    int D_created = 0;

    // Count the number of events where e.type == 'R'
    int i = 0;
    for (i = 0; i < event_queue->n; i++) {
        if (event_queue->Q[i].type == 'R') {
            left_reporter++;
        }
        if (event_queue->Q[i].type == 'E') {
            break;
        }
    }


    // Initialize the mutex and condition variables
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_trylock(&mutex);
    pthread_mutex_unlock(&mutex);

    pthread_mutex_init(&dummy, NULL);
    pthread_mutex_trylock(&dummy);
    pthread_mutex_unlock(&dummy);

    pthread_mutex_init(&done_mutex, NULL);
    pthread_mutex_trylock(&done_mutex);
    pthread_mutex_unlock(&done_mutex);

    pthread_mutex_init(&time_mutex, NULL);
    pthread_mutex_trylock(&time_mutex);
    pthread_mutex_unlock(&time_mutex);

    pthread_mutex_init(&patient_mutex, NULL);
    pthread_mutex_trylock(&patient_mutex);
    pthread_mutex_unlock(&patient_mutex);

    pthread_mutex_init(&reporter_mutex, NULL);
    pthread_mutex_trylock(&reporter_mutex);
    pthread_mutex_unlock(&reporter_mutex);

    pthread_mutex_init(&sales_rep_mutex, NULL);
    pthread_mutex_trylock(&sales_rep_mutex);
    pthread_mutex_unlock(&sales_rep_mutex);

    pthread_cond_init(&cond_done, NULL);
    pthread_cond_init(&cond_doctor_done, NULL);
    pthread_cond_init(&cond_reporter, NULL);
    pthread_cond_init(&cond_patient, NULL);
    pthread_cond_init(&cond_sales_rep, NULL);
    pthread_cond_init(&new_visitor, NULL);

    // Create the doctor thread
    pthread_t doctor_thread;
    pthread_create(&doctor_thread, NULL, doctor, (void *)&Doc_time);

    // While the event queue E is not empty
    while (!emptyQ(*event_queue)) {
        if(sales_rep_count >= 3 && patient_count >= 25 && left_reporter == 0){
            all_done = 1;
            if(doc_available) pthread_cond_signal(&new_visitor);
        }

        if(doc_available == 1 && visitor_done == 1 && D_created == 0 && all_done == 0){              // Creating D event
            if(!empty){
                // printf("Creating D event\n");
                // fflush(stdout);
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
            pthread_t visitor_thread;
            int maxCount;

            if (e.type == 'R') {
                visitorType = "Reporter";
                visitorCount = &reporter_count;
                (*visitorCount)++;
                int hours = 9 + arrival_time / 60;
                int minutes = arrival_time % 60;

                if (minutes < 0) {
                    minutes += 60;
                    hours--;
                }
                printf("\t\t\t[%02d:%02dam] %s %d arrives\n", hours, minutes, visitorType, *visitorCount);
                fflush(stdout);

                visitor_data *data = malloc(sizeof(visitor_data));
                data->number = *visitorCount;
                data->duration = e.duration;

                // Lock the mutex before accessing Doc_time
                pthread_mutex_lock(&time_mutex);
                data->accept_time = Doc_time;
                pthread_mutex_unlock(&time_mutex);

                if(empty) {empty = 0; printf("empty = 0\n"); fflush(stdout);}
                left_reporter--;
                reporter_wait_count++;
                pthread_create(&visitor_thread, NULL, reporter, data);
                pthread_mutex_unlock(&mutex);

            } else if (e.type == 'P') {
                visitorType = "Patient";
                visitorCount = &patient_count;
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
                    fflush(stdout);
                    
                    visitor_data *data = malloc(sizeof(visitor_data));
                    data->number = *visitorCount;
                    data->duration = e.duration;

                    // Lock the mutex before accessing Doc_time
                    pthread_mutex_lock(&time_mutex);
                    data->accept_time = Doc_time;
                    pthread_mutex_unlock(&time_mutex);

                    if(empty) {empty = 0; printf("empty = 0\n"); fflush(stdout);}
                    patient_wait_count++;
                    pthread_create(&visitor_thread, NULL, patient, data);
                }
                else {
                    printf("\t\t\t[%02d:%02dam] %s %d arrives\n", hours, minutes, visitorType, *visitorCount);
                    fflush(stdout);
                    printf("\t\t\t[%02d:%02dam] %s %d leaves (quota full)\n", hours, minutes, visitorType, *visitorCount);
                    fflush(stdout);
                }
                pthread_mutex_unlock(&mutex);

            } else if(e.type == 'S') {
                visitorType = "Sales representative";
                visitorCount = &sales_rep_count;
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
                    fflush(stdout);
                    
                    visitor_data *data = malloc(sizeof(visitor_data));
                    data->number = *visitorCount;
                    data->duration = e.duration;

                    // Lock the mutex before accessing Doc_time
                    pthread_mutex_lock(&time_mutex);
                    data->accept_time = Doc_time;
                    pthread_mutex_unlock(&time_mutex);

                    if(empty) {empty = 0; printf("empty = 0\n"); fflush(stdout);}
                    sales_rep_wait_count++;
                    pthread_create(&visitor_thread, NULL, salesrep, data);
                }
                else {
                    printf("\t\t\t[%02d:%02dam] %s %d arrives\n", hours, minutes, visitorType, *visitorCount);
                    fflush(stdout);
                    printf("\t\t\t[%02d:%02dam] %s %d leaves (quota full)\n", hours, minutes, visitorType, *visitorCount);
                    fflush(stdout);
                }
                pthread_mutex_unlock(&mutex);

            }
        } else if (e.type == 'D') {
            pthread_mutex_lock(&mutex);

            // printf("Handling D event\n");
            // fflush(stdout);
            pthread_mutex_lock(&time_mutex);
            Doc_time = e.time;
            pthread_mutex_unlock(&time_mutex);

            if(reporter_wait_count > 0 || patient_wait_count > 0 || sales_rep_wait_count > 0){
                // printf("\nSignaling doctor\n");
                // fflush(stdout);

                // Decrement the semaphore
                sem_wait(&sem);

                pthread_cond_signal(&new_visitor);

                if(reporter_wait_count > 0){
                    while (!reporter_ready) {
                        usleep(1000);  // Sleep for 1 millisecond
                    }
                    pthread_cond_signal(&cond_reporter);
                    // printf("Signaling reporter\n");
                    // fflush(stdout);
                    reporter_wait_count--;
                } else if(patient_wait_count > 0){
                    while(!patient_ready){
                        usleep(1000);
                    }
                    pthread_cond_signal(&cond_patient);
                    // printf("Signaling patient\n");
                    // fflush(stdout);
                    patient_wait_count--;
                } else if(sales_rep_wait_count > 0){
                    while(!sales_rep_ready){
                        usleep(1000);
                    }
                    pthread_cond_signal(&cond_sales_rep);
                    // printf("Signaling sales representative\n");
                    // fflush(stdout);
                    sales_rep_wait_count--;
                }

                // For the doctor and visitor both to finish
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
                // printf("no one is waiting\n");
                // fflush(stdout);
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
    pthread_cond_destroy(&cond_reporter);
    pthread_cond_destroy(&cond_patient);
    pthread_cond_destroy(&cond_sales_rep);
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&time_mutex);
    pthread_mutex_destroy(&done_mutex);
    pthread_cond_destroy(&cond_done);
    pthread_cond_destroy(&cond_doctor_done);
    pthread_cond_destroy(&new_visitor);
    pthread_mutex_destroy(&dummy);


    pthread_exit(NULL);
}

void *doctor(void *arg) {
    int count = 0;
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
            fflush(stdout);
            pthread_mutex_unlock(&time_mutex);
            pthread_exit(NULL);
        }
        doc_available = 1;
        // printf("Doctor is waiting for a visitor\n");
        // fflush(stdout);
        pthread_mutex_lock(&dummy);   
        pthread_cond_wait(&new_visitor, &dummy);
        done_doctor = 0;
        if(all_done == 1){
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
        fflush(stdout);
        // printf("Count = %d\n", ++count);
        // fflush(stdout);

        pthread_mutex_lock(&done_mutex);
        done_counter++;
        pthread_mutex_unlock(&done_mutex);

        done_doctor = 1;

        pthread_cond_signal(&cond_doctor_done);  // Signal that the doctor is done
        pthread_cond_signal(&cond_done);
        pthread_mutex_unlock(&dummy);

    }
}

void *patient(void *arg) {
    visitor_data *data = (visitor_data *)arg;
    int number = data->number;
    int accept_time = data->accept_time;
    int duration = data->duration;

    // Lock the mutex before waiting on the condition variable
    pthread_mutex_lock(&patient_mutex);
    patient_ready = 0;

    // printf("Patient %d created\n", number);
    // fflush(stdout);

    // Increment the semaphore
    sem_post(&sem);

    // printf("Patient %d is about to wait\n", number);
    // fflush(stdout);

    patient_ready = 1;
    // Wait on the condition variable
    pthread_cond_wait(&cond_patient, &patient_mutex);
    patient_ready = 0;

    // printf("Patient %d is now ready\n", number);
    // fflush(stdout);

    // After being signaled and waking up, calculate the end time
    pthread_mutex_lock(&time_mutex);
    // printf("Patient %d has escaped the time_mutex\n", number);
    // fflush(stdout);
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
    if(!done_doctor) pthread_cond_wait(&cond_doctor_done, &patient_mutex);  // Wait for the doctor to finish
    printf("[%02d:%02dam - %02d:%02dam] Patient %d is in doctor's chamber\n", hours_accept, minutes_accept, hours_end, minutes_end, number);
    fflush(stdout);
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

    reporter_ready = 0;
    // printf("Reporter %d created\n", number);
    // fflush(stdout); 

    // Increment the semaphore
    sem_post(&sem);

    // printf("Reporter %d is about to wait\n", number);
    // fflush(stdout);

    reporter_ready = 1;
    // Wait on the condition variable
    pthread_cond_wait(&cond_reporter, &reporter_mutex);
    reporter_ready = 0;

    // printf("Reporter %d is now ready\n", number);
    // fflush(stdout);

    // After being signaled and waking up, calculate the end time
    pthread_mutex_lock(&time_mutex);
    // printf("Reporter %d has escaped the time_mutex\n", number);
    // fflush(stdout);
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
    if(!done_doctor) pthread_cond_wait(&cond_doctor_done, &patient_mutex);  // Wait for the doctor to finish
    printf("[%02d:%02dam - %02d:%02dam] Reporter %d is in doctor's chamber\n", hours_accept, minutes_accept, hours_end, minutes_end, number);
    fflush(stdout);

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
    sales_rep_ready = 0;

    // printf("Sales representative %d created\n", number);
    // fflush(stdout);

    // Increment the semaphore
    sem_post(&sem);

    // printf("Sales representative %d is about to wait\n", number);
    // fflush(stdout);

    sales_rep_ready = 1;
    // Wait on the condition variable
    pthread_cond_wait(&cond_sales_rep, &sales_rep_mutex);
    sales_rep_ready = 0;

    // printf("Sales representative %d is now ready\n", number);
    // fflush(stdout);

    // After being signaled and waking up, calculate the end time
    pthread_mutex_lock(&time_mutex);
    // printf("Sales representative %d has escaped the time_mutex\n", number);
    // fflush(stdout);
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
    if(!done_doctor) pthread_cond_wait(&cond_doctor_done, &patient_mutex);  // Wait for the doctor to finish
    printf("[%02d:%02dam - %02d:%02dam] Sales representative %d is in doctor's chamber\n", hours_accept, minutes_accept, hours_end, minutes_end, number);
    fflush(stdout);

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
