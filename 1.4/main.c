#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>

int main(){

    int fed[2];
    
    if (pipe(fed) == -1) {// we create the pipe frontend-dispatcher will use for communication
        perror("pipe");
    }

    pid_t fe, d;
    
    fe = fork();// we create the process frontend

    if(fe == 0){

        d = fork();// we create the process dispatcher
        
        if(d>0){
            frontend_init(d, fed);
        }
        else if(d == 0){
            dispatcher_init(getppid(), fed);
        }  
    }

    // the parent waits for the two processes to finish before he returns 0
    wait(NULL);
    wait(NULL);

    return 0;
}
