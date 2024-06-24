#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define COMMAND 'C'
#define EXECUTE 'E'

void parentmain ()
{
   int pfd[2], afd[2];
   pid_t cpid1, cpid2;
   char pfdin[32], pfdout[32], ackin[32], ackout[32];

   printf("+++ CSE in supervisor mode: Started\n");

   pipe(pfd);
   sprintf(pfdin, "%d", pfd[0]);
   sprintf(pfdout, "%d", pfd[1]);
   printf("+++ CSE in supervisor mode: pfd = [%d %d]\n", pfd[0], pfd[1]);

   pipe(afd);
   sprintf(ackin, "%d", afd[0]);
   sprintf(ackout, "%d", afd[1]);

   printf("+++ CSE in supervisor mode: Forking first child in command-input mode\n");
   cpid1 = fork();
   if (!cpid1) {
      execlp("xterm", "xterm", "-T", "First Child", "-e", "./CSE", "C", pfdin, pfdout, ackin, ackout, NULL);
   }

   printf("+++ CSE in supervisor mode: Forking second child in execute mode\n");
   cpid2 = fork();
   if (!cpid2) {
      execlp("xterm", "xterm", "-T", "Second Child", "-e", "./CSE", "E", pfdin, pfdout, ackin, ackout, NULL);
   }

   waitpid(cpid1, NULL, 0);
   printf("+++ CSE in supervisor mode: First child terminated\n");

   waitpid(cpid2, NULL, 0);
   printf("+++ CSE in supervisor mode: Second child terminated\n");

   exit(0);
}

void childmain ( char mode, int pfd0, int pfd1, int afd0, int afd1 )
{
   int fd0copy, fd1copy, argc;
   char buffer[4096], ack[2], *argv[128], token[1024], *cp;
   pid_t cpid;

   fd0copy = dup(0);
   fd1copy = dup(1);

   if (mode == COMMAND) {
      close(1); dup(pfd1);
   } else if (mode == EXECUTE) {
      close(0); dup(pfd0);
   } else {
      fprintf(stderr, "*** Unknown mode: Should be C or E\n");
      exit(3);
   }

   while (1) {
      if (mode == COMMAND) {
         fprintf(stderr, "Enter command> "); fflush(stderr);
         fgets(buffer, 4096, stdin);
         printf("%s", buffer);
         fflush(stdout);
         if (!strcmp(buffer, "exit\n")) break;
         if (!strcmp(buffer, "swaprole\n")) {
            close(1); dup(fd1copy);
            close(0); dup(pfd0);
            mode = EXECUTE;
            read(afd0, ack, 2);
            continue;
         }
      } else {
         fprintf(stdout, "Waiting for command> "); fflush(stdout);
         fgets(buffer, 4096, stdin);
         fprintf(stdout, "%s", buffer); fflush(stdout);
         if (!strcmp(buffer, "exit\n")) break;
         if (!strcmp(buffer, "swaprole\n")) {
            close(0); dup(fd0copy);
            close(1); dup(pfd1);
            mode = COMMAND;
            write(afd1, "Y", 2);
            continue;
         }
         cp = buffer;
         while ( (*cp == ' ') || (*cp == '\t') ) ++cp;
         argc = 0;
         while (*cp != '\n') {
            sscanf(cp, "%s", token);
            argv[argc] = (char *)malloc((strlen(token) + 1) * sizeof(char));
            strcpy(argv[argc], token);
            ++argc;
            cp += strlen(token);
            while ( (*cp == ' ') || (*cp == '\t') ) ++cp;
         }
         argv[argc] = NULL;
         if (argc == 0) continue;
         cpid = fork();
         if (cpid) {
            waitpid(cpid, NULL, 0);
         } else {
            close(0); dup(fd0copy);
            execvp(argv[0], argv);
            fprintf(stderr, "*** Unable to execute command\n");
            exit(4);
         }
      }
   }

   exit(0);
}

int main ( int argc, char *argv[] )
{
   if ((argc == 1) || (!strcmp(argv[1], "S"))) parentmain();
   if ( (!strcmp(argv[1],"C")) || (!strcmp(argv[1],"E")) ) {
      if (argc != 6) {
         fprintf(stderr, "*** Invalid number of arguments for child process\n");
         exit(1);
      }
      childmain(argv[1][0], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
   }
   fprintf(stderr, "*** Unknown mode: Should be one of C, S, and E\n");
   exit(2);
}