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
#include <errno.h>
#include <sys/select.h>  
#include <sys/time.h>

void worker_init(int fd, int readpipe, int writepipe, int x[]) {

    close(x[0]);// we close the read end
    close(writepipe);// we close the write end

    ssize_t bytesRead1, bytesRead2;
    char input[1024];
    off_t offset_diff;

    while (1) {

        memset(input, 0, sizeof(input));
        bytesRead2 = read(readpipe, input, sizeof(input) - 1);// the worker recieves the number of bytes he will read from the .txt

        if ((atoi(input) < 1) || (bytesRead2 < 0)) {
            continue;
        }

        char buffer[1024];
        offset_diff = (off_t)strtoll(input, NULL, 10);

        memset(buffer, 0, sizeof(buffer));
        bytesRead1 = read(fd, buffer, offset_diff);// he reads from the file the number of bytes he was told

        if (bytesRead1 == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        int counter = 0;

        for (int j = 0; j < bytesRead1; j++) {
            if (buffer[j] == 'x') {
                counter++;// everytime he finds the char 'x' the counter++
            }
        }

        write(x[1], &counter, sizeof(int));// he then sends to the dispatcher the counter

    }
}
