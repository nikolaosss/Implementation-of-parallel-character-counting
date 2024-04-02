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

void worker_init(int fd, int readpipe, int writepipe) {

    ssize_t bytesRead1, bytesRead2;
    char input[1024];
    off_t offset_diff;

    while (1) {

        memset(input, 0, sizeof(input));
        bytesRead2 = read(readpipe, input, sizeof(input) - 1);

        if ((atoi(input) < 1) || (bytesRead2 == -1)) {
            continue;
        }

        char buffer[1024];
        offset_diff = (off_t)strtoll(input, NULL, 10);
        printf("Offset diff: %" PRIiMAX "\n", (intmax_t)offset_diff);

        memset(buffer, 0, sizeof(buffer));
        bytesRead1 = read(fd, buffer, offset_diff);

        printf("i am reading: %s -- ", buffer);
        fflush(stdout);

        if (bytesRead1 == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        int counter = 0;

        for (int j = 0; j < bytesRead1; j++) {
            if (buffer[j] == 'x') {
                counter++;
            }
        }

        char message[1024];
        snprintf(message, sizeof(message), "%d", counter);

        write(writepipe, message, strlen(message) + 1);

        usleep(100);
        
    }
}
