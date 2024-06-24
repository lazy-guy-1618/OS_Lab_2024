#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int inverted = 0;

int main(int argc, char *argv[])
{
    int stdout_copy = dup(1); // Copy stdout
    int stdin_copy = dup(0);  // Copy stdin
    if (argc == 1)
    { // Parent process
        int pfd1[2];
        pipe(pfd1);
        int pfd2[2];
        pipe(pfd2);
        char fd1_write[10];
        char fd1_read[10];
        char fd2_write[10];
        char fd2_read[10];
        sprintf(fd1_write, "%d", pfd1[1]); // Convert the file descriptor to a string
        sprintf(fd1_read, "%d", pfd1[0]);
        sprintf(fd2_write, "%d", pfd2[1]);
        sprintf(fd2_read, "%d", pfd2[0]);
        printf("+++ CSE in supervisor mode: Started\n+++ CSE in supervisor mode: pfd = [%d %d]\n", pfd1[0], pfd1[1]);
        if (fork() == 0)
        { // Child process for C_terminal
            printf("\n+++ CSE in supervisor mode: Forking first child in command-input mode\n");
            execlp("xterm", "xterm", "-T", "C_terminal", "-e", "./CSE", "C", fd1_write, fd1_read, fd2_write, fd2_read, NULL);
            // exit(0);
        }
        else {
            if (fork() == 0)
            { // Child process for E_terminal
                printf("+++ CSE in supervisor mode: Forking second child in execute mode\n");
                execlp("xterm", "xterm", "-T", "E_terminal", "-e", "./CSE", "E", fd1_write, fd1_read, fd2_write, fd2_read, NULL);
                // exit(0);
            }
            else {
                wait(NULL); // Wait for the first child to finish
                wait(NULL); // Wait for the second child to finish
                printf("+++ CSE in supervisor mode: First child terminated\n");
                printf("+++ CSE in supervisor mode: Second child terminated\n");
            }
        }

    }

    if (argc > 1)
    {
        char mode[2];
        strcpy(mode, argv[1]);
        // printf("mode = %s, %d, %d", mode, strcmp(mode, "C"), strcmp(mode, "E"));
        if (strcmp(mode, "C")==0)
        {
            // printf("Found C!!!!");
            while (1)
            {   
                if (!inverted)
                {
                    close(0);
                    dup(stdin_copy);   // Duplicate stdin   
                    close(1);          // Close stdout
                    dup(stdout_copy); // Duplicate the write end of the pipe to stdout
                    close(1); // Close stdout
                    dup(atoi(argv[2])); // Duplicate the write end of the pipe to stdout
                    int pfd1[2];
                    pfd1[1] = atoi(argv[2]); // Convert the command-line argument to an integer
                    char command[256];
                    while (1)
                    {
                        fprintf(stderr, "Enter command> ");           // Display message on stderr
                        fgets(command, 256, stdin);                   // Read command from stdin
                        write(pfd1[1], command, strlen(command) + 1); // Write command to the pipe
                        if (strcmp(command, "exit\n") == 0)
                        {
                            exit(0); // Exit the process if the command is "exit"
                        }
                        else if (strcmp(command, "swaprole\n") == 0)
                        {
                            inverted += 1;
                            inverted %= 2;
                            break;
                        }
                    }
                }
                else if (inverted)
                {
                    close(0);
                    dup(stdin_copy);   // Duplicate stdin   
                    close(1);          // Close stdout
                    dup(stdout_copy);
                    close(0);           // Close stdin
                    dup(atoi(argv[5])); // Duplicate the read end of the pipe to stdin
                    int pfd1[2];
                    pfd1[0] = atoi(argv[5]); // Convert the command-line argument to an integer

                    char command[256];
                    while (1)
                    {
                    fprintf(stdout, "Waiting for command : \n");
                    fflush(stdout);
                    int t = read(pfd1[0], command, 256);
                    if (t > 0)
                    {
                        if (strcmp(command, "exit\n") == 0)
                        {
                            exit(0); // Exit the E child process if the command is "exit"
                        }

                        if (strcmp(command, "swaprole\n") == 0)
                        {
                            inverted += 1;
                            inverted %= 2;
                            break;
                        }

                        if (fork() == 0)
                        {
                            close(0);        // Close stdin
                            dup(stdin_copy); // Duplicate stdin
                            execlp("/bin/sh", "/bin/sh", "-c", command, NULL);
                            // printf("%s\n",command);
                            exit(0);
                        }
                        else
                        {
                            wait(NULL); // Wait for the grandchild to finish
                        }
                    }
                    }
                }
            }
                return 0;
        }
        else if (strcmp(mode, "E") == 0)
        {
            // printf("found E!!!");
            while (1)
            {
                if (!inverted)
                {
                    close(0);
                    dup(stdin_copy);   // Duplicate stdin   
                    close(1);          // Close stdout
                    dup(stdout_copy);
                    close(0);           // Close stdin
                    dup(atoi(argv[3])); // Duplicate the read end of the pipe to stdin
                    // close(atoi(argv[2]));     // Close the unused read end
                    // close(atoi(argv[4]));     // Close the unused write end
                    // close(atoi(argv[5]));     // Close the unused write end
                    int pfd1[2];
                    pfd1[0] = atoi(argv[3]); // Convert the command-line argument to an integer

                    char command[256];
                    while (1)
                    {
                        int flag = 0;
                        fprintf(stdout, "Waiting for command : \n");
                        fflush(stdout);
                        int t = read(pfd1[0], command, 256);
                        if (t > 0)
                        {
                            if (strcmp(command, "exit\n") == 0)
                            {
                                exit(0); // Exit the E child process if the command is "exit"
                            }

                            if (strcmp(command, "swaprole\n") == 0)
                            {
                                inverted += 1;
                                inverted %= 2;
                                break;
                            }

                            if (fork() == 0)
                            {
                                close(0);        // Close stdin
                                dup(stdin_copy); // Duplicate stdin
                                execlp("/bin/sh", "/bin/sh", "-c", command, NULL);
                                // printf("%s\n",command);
                                exit(0);
                            }
                            else
                            {
                                wait(NULL); // Wait for the grandchild to finish
                            }
                        }
                    }
                }
                else if (inverted)
                {
                    close(0);
                    dup(stdin_copy);   // Duplicate stdin   
                    close(1);          // Close stdout
                    dup(stdout_copy);


                    close(1); // Close stdout
                    dup(atoi(argv[4])); // Duplicate the write end of the pipe to stdout
                    
                    int pfd1[2];
                    pfd1[1] = atoi(argv[4]); // Convert the command-line argument to an integer
                    char command[256];
                    while (1)
                    {
                        fprintf(stderr, "Enter command> ");           // Display message on stderr
                        fgets(command, 256, stdin);                   // Read command from stdin
                        fprintf(stderr, "command = %s\n", command);
                        write(pfd1[1], command, strlen(command) + 1); // Write command to the pipe
                        if (strcmp(command, "exit\n") == 0)
                        {   
                            
                            exit(0); // Exit the process if the command is "exit"
                        }
                        else if (strcmp(command, "swaprole\n") == 0)
                        {
                            inverted += 1;
                            inverted %= 2;
                            break;
                        }
                    }
                }
            }

            return 0;
        }
    }
}
