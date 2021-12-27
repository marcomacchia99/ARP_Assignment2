#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

int fd_pipe;
int max_write_size;

pid_t producer_pid;

int size;
int mode;
struct rlimit limit;

//variables for select function
struct timeval timeout;
fd_set readfds;

void receive_array(char buffer[])
{
    //set timeout for select
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;

    //number of cycles needed to receive all the data
    int cycles = size / max_write_size + (size % max_write_size != 0 ? 1 : 0);

    for (int i = 0; i < cycles; i++)
    {
        //wait until data is ready
        do
        {
            //set timeout for select
            timeout.tv_sec = 0;
            timeout.tv_usec = 1000;
            
            FD_ZERO(&readfds);
            //add the selected file descriptor to the selected fd_set
            FD_SET(fd_pipe, &readfds);
        } while (select(FD_SETSIZE + 1, &readfds, NULL, NULL, &timeout) < 0);

        //read random string from producer
        char segment[max_write_size];
        read(fd_pipe, segment, max_write_size);
        // printf("read: %ld\n", read(fd_pipe, segment, size));

        //add every segment to entire buffer
        if (i == cycles - 1)
        {
            int j = 0;
            while ((i * max_write_size + j) < size)
            {
                buffer[i * max_write_size + j] = segment[j];
                j++;
            }
        }
        else
        {

            strcat(buffer, segment);
        }
    }
    // FILE *file = fopen("cons.txt", "w");
    // fprintf(file, "%s", buffer);
    // fflush(file);
    // fclose(file);
}

int main(int argc, char *argv[])
{
    //getting size from console
    if (argc < 2)
    {
        fprintf(stderr, "Consumer - ERROR, no size provided\n");
        exit(0);
    }
    size = atoi(argv[1]) * 1000000;

    //getting mode from console
    if (argc < 3)
    {
        fprintf(stderr, "Consumer - ERROR, no mode provided\n");
        exit(0);
    }
    mode = atoi(argv[2]);

    //defining fifo path
    char *fifo_named_pipe = "/tmp/named_pipe";
    char *fifo_named_producer_pid = "/tmp/named_producer_pid";

    //create fifo
    mkfifo(fifo_named_pipe, 0666);
    mkfifo(fifo_named_producer_pid, 0666);

    //receiving pid from producer
    int fd_pid_producer = open(fifo_named_producer_pid, O_RDONLY);

    int sel_val;
    do //wait until pid is ready
    {
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;

        FD_ZERO(&readfds);
        //add the selected file descriptor to the selected fd_set

        FD_SET(fd_pid_producer, &readfds);
        sel_val = select(FD_SETSIZE + 1, &readfds, NULL, NULL, &timeout);
    } while (sel_val <= 0);
    read(fd_pid_producer, &producer_pid, sizeof(producer_pid));
    close(fd_pid_producer);
    unlink(fifo_named_producer_pid);

    //open fifo
    fd_pipe = open(fifo_named_pipe, O_RDONLY);

    //defining max size for operations and files
    getrlimit(RLIMIT_NOFILE, &limit);
    max_write_size = limit.rlim_max;
    fcntl(fd_pipe, F_SETPIPE_SZ, max_write_size);

    //switch between dynamic allocation or standard allocation
    if (mode == 0)
    {
        //dynamic allocation of buffer
        char *buffer = (char *)malloc(size);

        //receive array from producer
        receive_array(buffer);

        //delete buffer from memory
        free(buffer);
    }
    else
    {
        //increasing stack limit to let the buffer be instantieted correctly
        limit.rlim_cur = (size + 5) * 1000000;
        limit.rlim_max = (size + 5) * 1000000;
        setrlimit(RLIMIT_STACK, &limit);

        char buffer[size];
        //receive array from producer
        receive_array(buffer);
    }

    //transfer complete. Sends signal to notify the producer
    kill(producer_pid, SIGUSR1);

    //close and delete fifo
    close(fd_pipe);
    unlink(fifo_named_pipe);

    return 0;
}