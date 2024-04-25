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

int type = 0, no_change = 0, counter = 0,after_process_call = 0,number = 0, count = 0;
double percentage_read = 0.0;
off_t  offset = 0, offset_step, temp_offset = 0;

void signal_add(int signum){
    type = 1;//type=1  we want to go to the if() where we are adding workers 
    no_change = 0;//we want recalculate everything about offset_step etc in the add if()
    counter = 0;//using counter&overall_workers, to distribute the file in a circular way
    after_process_call = 0;//because we want to receive data from the frontend
}
void signal_remove(int signum){
    type = -1;
    counter = 0;
    no_change = 1;// we recalculating everything in the remove if(), so we dont want to do it again in the add if()
    after_process_call = 0;
}
void signal_process(int signum){
    no_change = 1;// we dont have to recalculate anything about offset_step etc
    type = 10;
    after_process_call = 1;//because we dont want to receive any data from the frontend
}
void sigchld_handler(int signum) {
    type = -1;//type=-1, we want to go to the if() where we are removing workers
    counter = 0;
    no_change = 1;// we recalculating everything in the remove if(), so we dont want to do it again in the add if()
    after_process_call = 1;//because we dont want to receive any data from the frontend  
    number = 1;//after we kill the child, we arent going to the receive any data,so we skip the part where we gave the number variable a value, so we are updating the number here   
}

void calculate(int fd, off_t original_size, int **pipefd, int overall_workers, int cc){

    char message[1024];
    static off_t temp_remainder = 0;

    //if we have a remainder(cc=1), and offset reaches a certain number, we then distribute the rest of the bytes by giving one byte to each worker in a circular way until the whole file is read
    if((cc == 1) && (offset == temp_offset + offset_step*5*overall_workers + temp_remainder)){

        while(percentage_read<100.0){

            sleep(2);

            //if a signal is triggered while we are in this loop, the flag will be updated to the value 1, so we exit the loop
            if(type != 0){
                break;
            }

            lseek(fd, offset, SEEK_SET);//we are moving the file pointer to where we have read until this poing
            offset++;//now offset is incemented by one each time, cause each workers reads one byte 
            temp_remainder++;
            off_t r = 1;
            
            snprintf(message, sizeof(message), "%" PRIiMAX, (intmax_t)r);

            write(pipefd[counter%overall_workers][1], message, strlen(message) + 1);

            percentage_read = ((double)offset / (double)original_size) * 100.0;

            count++;
            counter++;

            printf("-- -- \n");
        }

    }
    else{

        lseek(fd, offset, SEEK_SET);
        
        offset += offset_step;//we are updating the offset each time a worker is reading his part which is offset_step

        snprintf(message, sizeof(message), "%" PRIiMAX, (intmax_t)offset_step);//we format an integer to a string
        
        write(pipefd[counter%overall_workers][1], message, strlen(message) + 1);//sending the workers the bytes the need to read

        percentage_read = ((double)offset / (double)original_size) * 100.0;//calculating based on the data, the percentage of the file we have read
        
        count++;//we are incrementing each time a worker is sending the dipatcher an integer
        counter++;
    }   
}

void find_step(int overall_workers, off_t original_size, int *cc, int *remainder, off_t *file_size){

    temp_offset = offset;

    *file_size = original_size - offset;//how much of the file we have not read
    offset_step = (*file_size) / (overall_workers*5);//the new offset_step

    *remainder = *file_size % (overall_workers*5);

    if(overall_workers*5 > *file_size) {
        offset_step = 1;
        *remainder = 0;
    }

    if(*remainder > 0){*cc = 1;}//if we have remainder, cc=1
    else{*cc = 0;}//else, cc=0
}

int dispatcher_init(pid_t f, int fed [], char filename[], char character){

    char buffer[1024];
    int temp = 0;
    int overall_workers = 0;
    int cc = 0;
    int result = 0;
    int remainder = 0;
    int ricta = 0;
    pid_t *p;
    int x[2];

    if (pipe(x) == -1) {//this pipe is for the workers to send here the number of times they found the char we are looking for each time
        perror("pipe");
    }

    unsigned int capacity = 20;

    p = (pid_t *)malloc(capacity * sizeof(pid_t));

    if (p == NULL) {
        printf("Memory allocation failed\n");
        return 1;
    }
    int** pipefd = (int**)malloc(capacity * sizeof(int*));

    for (int k = 0; k < 20; k++){
        pipefd[k] = (int*)malloc(2 * sizeof(int));
    }

    int fd = open(filename, O_RDONLY);// we open the txt file and getting the fd

    off_t file_size = lseek(fd, 0, SEEK_END);// we are finding the size of the file we are about to search
    off_t original_size = file_size;

    if (file_size == -1) {
        perror("lseek");
        exit(EXIT_FAILURE);
    }

    signal(SIGUSR1, signal_add);//if the command wants to add a number of workers, this siganl is triggered
    signal(SIGUSR2, signal_remove);//if the command wants to remove a number of workers, this siganl is triggered
    signal(SIGRTMIN, signal_process);//if the command wants to know the process until now, this signal is triggered
    signal(SIGCHLD, sigchld_handler);//if a child is been terminated by using the kill order, this signal is triggered

    while(1){

        //if the process siganl is triggered or the signal that a child has dies, we dont want to go to this if cause we arent waiting any data from the frontend
        if(after_process_call == 0){
       
            number = 0;

            //we receive from the frontend the instruction(add or remove or send him the current process)
            int bytes_read = read(fed[0], buffer, sizeof(buffer));
   
            if(strcmp(buffer, "process") != NULL){

                for (int i = 0; i<bytes_read; i++) {
                    if (buffer[i] >= '0' && buffer[i] <= '9') {

                        number = number * 10 + (buffer[i] - '0');

                    }
                    else{break;}
                }
                if(number == 0){continue;}
                if((type == -1) && (overall_workers - number)<0){printf("--You can not have negative workers, right now you have %d workers, try a different input--\n", overall_workers);fflush(stdout);continue;}
            }
        }
        if(type == 1){//this if is for the case when we want to add workers

            type--;
            
            //if we want to check the process when we are already have some workers searching, we dont want to recalculate everything 
            if(no_change == 0){

                temp = overall_workers;
                overall_workers += number;//we update the overall workers
                if(overall_workers > capacity){
                    unsigned int new_capacity = 2*overall_workers;
                    printf("resize\n");
                    p = realloc(p,new_capacity*sizeof(pid_t));
                    pipefd = realloc(pipefd, new_capacity*sizeof(int*));
                    for(int i = capacity; i < new_capacity; ++i){
                        pipefd[i] = (int*)malloc(2*sizeof(int));
                    }
                    capacity = new_capacity;
                }
                //here we are creating one pipe for every new worker we are adding
                for (int i = 0; i < number; i++) {
                    if (pipe(pipefd[temp+i]) == -1) {
                        perror("pipe");
                    }
                }

                //here we the new data, we are going to calculate how much each worker will read each time
                find_step(overall_workers, original_size, &cc, &remainder, &file_size);

                //with fork we are create one process for every worker and we are calling the worker_init funtion
                for(int i = 0;i<number;i++){

                    p[i+temp] = fork();
                    
                    if(p[i+temp] == -1){
                        perror("fork");
                        return 1;
                    }
                    else if(p[i+temp] == 0){
                        //the parameters we are giving to this function are the fd, the read end of the worker-dispatcher pipe and the write end of the pipe x
                        worker_init(fd,pipefd[temp+i][0],pipefd[temp+i][1], x, character);
                        exit(0);
                    }
                }
            }

            while(percentage_read < 100.0){

                sleep(3);

                //if a signal is triggered while we are in this loop, the flag will be updated to the value 1, so we exit the loop
                if(type != 0){
                    break;
                }

                //here, the dispatcher in a circular way, will send to each worker what he have to read from the file
                calculate(fd, original_size, pipefd, overall_workers, cc);

                printf("-- -- \n");

                //when the whole file is read and we exit the if, we will go to the process if to print the final data
                if(percentage_read == 100.0){
                    after_process_call = 1;
                    type = 10;
                }

            }   
        }
        else if(type == -1){//this if is for the case when we want to remove workers 

            type = 1;

            overall_workers -= number;

            find_step(overall_workers, original_size, &cc, &remainder, &file_size);

            after_process_call = 1;


        }
        else if(type == 10){//this if is for the case where we want to find the current process

            // we are closing the read end of the pipe x, because the dispatcher will only receive data from this pipe 
            close(x[1]);

            type = type - 10;
            int received_number = 0;

            //count is telling us how many times overall an integer has been sent through pipe x. we then receive the data and adding it to the ricta variable
            for(int w = 0;w<count;w++){

                read(x[0], &received_number, sizeof(int));
                ricta += received_number;

            }
            
            count = 0;
            char to_front [1024];

            //we are triggering a signal in the frontend(we are using his PID), to inform him that we are about to send him the data about the current state 
            kill(f, SIGUSR1);

            //we first send him the percentage that was read 
            snprintf(to_front, sizeof(to_front), "%lf", percentage_read);
            write(fed[1], to_front, strlen(to_front) + 1);

            usleep(10);

            //and here we are sending him the number of times the char 'x' was found
            snprintf(to_front, sizeof(to_front), "%d", ricta);
            write(fed[1], to_front, strlen(to_front) + 1);

            if(percentage_read!=100){
                type = 1;
            }

            //if the whole file has been read, we are releasing the memory we had previously binded
            if(percentage_read == 100.0){
                free(p);
                for(int i =0;i<capacity;++i) free(pipefd[i]);
                free(pipefd);
                for(int i = 0;i<overall_workers;++i){
                    kill(p[i],SIGINT);
                }
                exit(0);
            }


        }
    }
}