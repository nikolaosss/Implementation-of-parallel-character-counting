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

int overall_workers = 0, type = 0, no_change = 0, counter = 0, cc = 0,flag = 0,after_process_call = 0;
double percentage_read = 0.0,remainder_perce = 0;
off_t  current_offset = 0, offset = 0;

void signal_handler1(int signum){
    type = 1;
    flag = 1;// flag = 1, so this means we exit the loop 
    no_change = 0;// we are adding so we need to create new pipes etc
    counter = 0;// the while loop now that we are adding we start from 0
}
void signal_handler2(int signum){
    type = -1;
    flag = 1;
    counter = 0;
    no_change = 1;
}
void signal_handler3(int signum){
    no_change = 1;// while the workers are searching we are calling process to check the status but we dont want to initialize type == 1
    type = 10;
    flag = 1;
    after_process_call = 1;
}

int dispatcher_init(int fed []){
    
    char buffer;
    off_t file_size, offset_step;
    int number = 0,temp = 0,bytes_read = 0,ricta = 0;
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

    file_size = lseek(fd, 0, SEEK_END);
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
            bytes_read = read(fed[0], &buffer, 1);
        }

        flag = 0;

        if(type == 1){

            type--;
            
            if(no_change == 0){

                number = buffer - '0';

                temp = overall_workers;
                overall_workers += number;
                
                for (int i = 0; i < number; i++) {
                    if (pipe(pipefd[temp+i]) == -1) {
                        perror("pipe");
                    }
                }

                file_size -= offset;// file_size -= offset, because we already have read a portion of the file(offset)
                offset_step = (file_size) / (overall_workers*5);

                remainder = file_size % (overall_workers);

                remainder_perce = ((double)remainder / (double)original_size) * 100.0;

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

            printf("file_size %" PRIiMAX "\n", (intmax_t)file_size);
            printf("file_offset %" PRIiMAX "\n", (intmax_t)offset);

            while(counter < overall_workers*5){

                sleep(2);
                
                if(flag == 1){
                    break;
                }

                if(percentage_read > 100.0){
                    break;
                }

                char message[1024];

                if((cc == 1) && (counter == (overall_workers*4))){
                    while(percentage_read<100.0){
                        
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

                        printf("Percentage of file read: %.2f%%\n", percentage_read);
                        printf("Offset value: %" PRIiMAX "\n", (intmax_t)offset);

                        fflush(stdout);
                        counter++;
                    }
                    break;
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
                    
                    printf("Percentage of file read: %.2f%%\n", percentage_read);
                    printf("Offset value: %" PRIiMAX "\n", (intmax_t)offset);

                    fflush(stdout);
                }
                counter++;
            }
            
        }
        else if(type == -1){

            type++;

            number = buffer - '0';

            temp = overall_workers;
            overall_workers -= number;
            
            for (int i = 0; i < number; i++) {
                free(pipefd[temp-i]);
                free(p[temp-i]);
            }

            file_size -= offset;// file_size -= offset, because we already have read a portion of the file(offset)
            offset_step = (file_size) / (overall_workers*5);

            remainder = file_size % (overall_workers);

            remainder_perce = ((double)remainder / (double)original_size) * 100.0;

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
        
            printf("percentage_read: %.2f%%. ", percentage_read);
            printf("The character was found: %d times", ricta);
        
            fflush(stdout);

            after_process_call = 0;

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
