#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>

int pipefd[2];


int valid_input(char buff[], int *numDigits){

    int w = 0;

    char worker_or_workers[10];

    if(strcmp(buff, "process") == 0){
        return 1;
    }
    else if (sscanf(buff, "add %d %9s", &w, worker_or_workers) == 2) {
        if ((w > 1 && strcmp(worker_or_workers, "workers") == 0) ||
            (w == 1 && strcmp(worker_or_workers, "worker") == 0)) {
            *numDigits = snprintf(NULL, 0, "%d", w);
            return 1;
        }
        else{return 0;}
    }
    else if (sscanf(buff, "remove %d %9s", &w, worker_or_workers) == 2) {
        if ((w > 1 && strcmp(worker_or_workers, "workers") == 0) ||
            (w == 1 && strcmp(worker_or_workers, "worker") == 0)) {
            *numDigits = snprintf(NULL, 0, "%d", w);
            return 1;
        }
        else{return 0;}
    }
    else{
        return 0;
    }
    
}

void signal_handler4(int signum){

    char print[1024];

    double percentage;

    read(pipefd[0], print, sizeof(print));
    sscanf(print, "%lf", &percentage);
    printf("Percentage_read: %.2f%%. ", percentage);
    fflush(stdout);

    read(pipefd[0], print, sizeof(print));
    printf("The character was found: %d times\n", atoi(print));
    fflush(stdout);

    if(percentage == 100.0){exit(0);}

}

void frontend_init(pid_t dispatcher_pid, int fed[]){

    pipefd[0] = fed[0];

    signal(SIGUSR1, signal_handler4);

    char input[1024], buffer[1024], output[1024];

    while(1){

        int numDigits = 0;
        int j = 0;

        fgets(input, sizeof(input), stdin);

        if (input[strlen(input) - 1] == '\n') {
            input[strlen(input) - 1] = '\0';
        }

        if(strstr(input, "add") != NULL){

            if(valid_input(input, &numDigits)){
                for (int i = 0; i<numDigits; i++) {
                    j = j * 10 + (input[4+i] - '0');
                }
             
                snprintf(output, sizeof(output), "%d", j);
                write(fed[1], output, strlen(output) + 1);

                kill(dispatcher_pid, SIGUSR1);
                continue;
            }
            else{printf("--Invalid input, try again--\n");continue;}
        }
        else if(strstr(input, "remove") != NULL){

            if(valid_input(input, &numDigits)){
                for (int i = 0; i<numDigits; i++) {
                    j = j * 10 + (input[7+i] - '0');
                }

                snprintf(output, sizeof(output), "%d", j);
                write(fed[1], output, strlen(output) + 1);

                kill(dispatcher_pid, SIGUSR2);
                continue;
            }
            else{printf("--Invalid input, try again--\n");continue;}
        }
        else if(strstr(input, "process") != NULL){

            if(valid_input(input, &numDigits)){

                kill(dispatcher_pid, SIGINT);
                continue;

            }
            else{printf("--Invalid input, try again--\n");continue;}
        }
        else{ printf("--Invalid input, try again--\n");}
    }     
}


