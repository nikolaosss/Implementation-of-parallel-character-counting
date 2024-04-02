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
    
    if (pipe(fed) == -1) {
        perror("pipe");
    }

    pid_t fe, d;
    
    fe = fork();

    if(fe == 0){

        d = fork();
        
        if(d>0){
            frontend_init(d, fed);
        }
        else if(d == 0){
            dispatcher_init(getppid(), fed);
        }  
    }

    wait(NULL);
    wait(NULL);

    return 0;
}
