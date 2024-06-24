#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include<string.h>


int main(int argc, char *argv[]){
    char *city_name = (char *)malloc(128*sizeof(char));
    int level = 0;
    if(argc==1) {printf("Run with a node name.\n"); exit(0);}
    level = (argc > 2)? atoi(argv[2]) : 0;
    strcpy(city_name, argv[1]);
    FILE *fp = fopen("treeinfo.txt", "r");
    char buff[128];
    while(fgets(buff, 128, fp) != NULL){
        char* token  = strtok(buff, " ");
        if(strcmp(token, city_name) == 0){          //city found, now check the children and take care of the indentations
            
            //indentation
            for(int i = 0; i<level; ++i) printf("\t");
            int ppid = getpid();
            printf("%s (%d)\n", city_name, ppid);
            int children = atoi(strtok(NULL, " "));
            if(!children) exit(0);                  //No child found
            
            for(int i = 0; i<children; ++i){        // Traverse over all children
                char *child_city_name = strtok(NULL, " ");
                int t = strlen(child_city_name);
                if(child_city_name[t-1]=='\n') child_city_name[t-1]='\0';               // Making it null terminated instead of escape char terminated.
                pid_t pid = fork();                 // Now we fork !
                if(!pid){                           // Child process
                    char new_level[50];
                    int temp = level + 1;
                    sprintf(new_level, "%d", temp);
                    execlp("./proctree","./proctree", child_city_name, new_level, NULL);        // exec's with one more level
                    exit(0);
                }
                wait(NULL);                         // Parent waits for the child to finish
            }
            exit(0);                                // Parent finishes
        }

    }
    printf("City %s not found\n", city_name); exit(0);

    return 0;
}