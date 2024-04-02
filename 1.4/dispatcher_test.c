#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/select.h>  
#include <sys/time.h>

int type = 0, no_change = 0, counter = 0,flag = 0,after_process_call = 0, ricta = 0;
double percentage_read = 0.0;
off_t  current_offset = 0, offset = 0, offset_step, temp_offset = 0;

void signal_handler1(int signum){
    type = 1;
    flag = 1;
    no_change = 0;
    counter = 0;
    after_process_call = 0;
}
void signal_handler2(int signum){
    type = -1;
    flag = 1;
    counter = 0;
    no_change = 1;
    after_process_call = 0;
}
void signal_handler3(int signum){
    no_change = 1;
    type = 10;
    flag = 1;
    after_process_call = 1;
}

void calculate(int fd, off_t original_size, int **pipefd, int overall_workers, int cc){

    char message[1024];

    if((cc == 1) && (offset == temp_offset + offset_step*5*overall_workers)){

        while(percentage_read<100.0){

            sleep(2);
    
            current_offset = lseek(fd, offset, SEEK_SET);

            offset++;

            off_t r = 1;
            
            snprintf(message, sizeof(message), "%" PRIiMAX, (intmax_t)r);

            write(pipefd[counter%overall_workers][1], message, strlen(message) + 1);

            usleep(20);

            memset(message, 0, sizeof(message));
            int bytesRead2 = read(pipefd[counter%overall_workers][0], message, sizeof(message));

            ricta += atoi(message);
            percentage_read = ((double)offset / (double)original_size) * 100.0;
            printf("ricta%d\n",ricta);

            printf("Percentage of file read: %.2f%%\n", percentage_read);
            printf("Offset value: %" PRIiMAX "\n", (intmax_t)offset);   

            fflush(stdout);

            counter++;
        
        }

    }
    else{

        current_offset = lseek(fd, offset, SEEK_SET);
        
        offset += offset_step;

        snprintf(message, sizeof(message), "%" PRIiMAX, (intmax_t)offset_step);

        write(pipefd[counter%overall_workers][1], message, strlen(message) + 1);

        usleep(20);

        memset(message, 0, sizeof(message));
        int bytesRead2 = read(pipefd[counter%overall_workers][0], message, sizeof(message));

        ricta += atoi(message);
        percentage_read = ((double)offset / (double)original_size) * 100.0;

        printf("ricta%d\n",ricta);
        printf("Percentage of file read: %.2f%%\n", percentage_read);
        printf("Offset value: %" PRIiMAX "\n", (intmax_t)offset);

        fflush(stdout);
    }
    counter++;       
}

int dispatcher_init(pid_t f, int fed []){
    
    char buffer[1024];
    int temp = 0, number = 0, overall_workers = 0, cc = 0, result = 0;
    pid_t *p;

    long int remainder = 0;

    p = (pid_t *)malloc(20 * sizeof(pid_t));

    if (p == NULL) {
        printf("Memory allocation failed\n");
        return 1;
    }

    int** pipefd = (int**)malloc(20 * sizeof(int*));

    for (int k = 0; k < 20; k++){
        pipefd[k] = (int*)malloc(2 * sizeof(int));
    }

    int fd = open("1.txt", O_RDONLY);

    off_t file_size = lseek(fd, 0, SEEK_END);
    off_t original_size = file_size;

    if (file_size == -1) {
        perror("lseek");
        exit(EXIT_FAILURE);
    }

    signal(SIGUSR1, signal_handler1);
    signal(SIGUSR2, signal_handler2);
    signal(SIGINT, signal_handler3);

    while(1){
        
        if(after_process_call == 0){

            number = 0;

            int bytes_read = read(fed[0], buffer, sizeof(buffer));
   
            if(strcmp(buffer, "process") != NULL){

                for (int i = 0; i<bytes_read; i++) {
                    if (buffer[i] >= '0' && buffer[i] <= '9') {

                        number = number * 10 + (buffer[i] - '0');

                    }
                    else{break;}
                }
                if(number == 0){continue;}
            }
        }
        
        flag = 0;

        if(type == 1){

            type--;
            
            if(no_change == 0){

                temp = overall_workers;
                overall_workers += number;
                
                for (int i = 0; i < number; i++) {
                    if (pipe(pipefd[temp+i]) == -1) {
                        perror("pipe");
                    }
                }

                temp_offset = offset;

                file_size = original_size - offset;
                offset_step = (file_size) / (overall_workers*5);

                remainder = file_size % (overall_workers);

                if(overall_workers*5 > file_size) {
                    offset_step = 1;
                    remainder = 0;
                }

                if(remainder > 0){cc = 1;}

                if(remainder == 0){cc = 0;}

                for(int i = 0;i<number;i++){

                    p[i+temp] = fork();
                    
                    if(p[i+temp] == -1){
                        perror("fork");
                        return 1;
                    }
                    else if(p[i+temp] == 0){
                        worker_init(fd,pipefd[temp+i][0],pipefd[temp+i][1]);
                        exit(0);
                    }
                }
            }

            while(percentage_read < 100.0){

                sleep(3);
                
                if(flag == 1){
                    break;
                }

                calculate(fd, original_size, pipefd, overall_workers, cc);

                if(percentage_read == 100.0){
                    after_process_call = 1;
                }

            }   
        }
        else if(type == -1){

            type++;

            overall_workers -= number;

            temp_offset = offset;

            file_size = original_size - offset;
            offset_step = (file_size) / (overall_workers*5);

            remainder = file_size % (overall_workers);

            if(overall_workers*5 > file_size) {
                offset_step = 1;
                remainder = 0;
            }

            if(remainder > 0){cc = 1;}

            if(remainder == 0){cc = 0;}

            type++;

            after_process_call = 1;

        }
        else if(type == 10){

            type = type - 10;
            
            char to_front [1024];

            kill(f, SIGUSR1);

            snprintf(to_front, sizeof(to_front), "%lf", percentage_read);
            write(fed[1], to_front, strlen(to_front) + 1);

            usleep(10);

            snprintf(to_front, sizeof(to_front), "%d", ricta);
            write(fed[1], to_front, strlen(to_front) + 1);

            if(counter!=0){
                type = 1;
            }

            if(percentage_read == 100.0){
                free(p);
                free(pipefd);
                return 1;
            }
        }
    }
}
