/*
  Title          : redacted.c
  Author         : Mane Galstyan
  Created on     : May 6, 2024
  Description    : pthreads used to search file for patter
  Purpose        : To use mutex and pthreads
  Usage          : redacted <num of threads> <pattern> <input file> <output file>
  Build with     : gcc -o redacted redacted.c
  Info           : used mutexts to avoid race conditons when modifiying an array and varible and the main thread writes to the file
*/

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>

#define REDACT_CHARS "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_ "

pthread_mutex_t mutex;
int num_threads;
size_t pattern_size;
intmax_t file_size;
char* pattern;
char* file_buffer;
int count;

typedef struct x {
    int thread_id;
    int index;
} x;

x* found; /* hold the found indexes and thread id*/

/* prints error*/
void fatal_error(int errornum, const char *msg) {
    fprintf(stderr, "%s\n", msg);
    printf("%s\n",msg);
    exit(EXIT_FAILURE);
}

/* prints usage error*/
void usage_error(const char *msg) {
    fprintf(stderr, "Usage: %s", msg);
    printf("%s\n",msg);
    exit(EXIT_FAILURE);
}

/* struct to hold the location in file_buffer and thread_id*/
typedef struct _task_data
{
    int first;       /* index of first element for task */
    int last;        /* index of last element for task */
    int task_id;     /* id of thread */
} task_data;


/*
param:
 display[] array to hold the displament for the file_buffer
 distribute[] array that contains the number of values taken from file for each processor
 pattern_size is the size of pattern
 p is the number of processors
description:
 calculates the displacment based on the distrubution in the pattersize - 1 to account for pattern overlap for each processor
*/
void displacment(int displs[], int distribute[], size_t pattern_size, int p) {
    displs[0] = 0;
    for(int i = 1; i < p; i++) {
        displs[i] = displs[i - 1] + distribute[i - 1] - (pattern_size - 1);
    }
}

/*
param:
 file_size which is size of file
 pattern_size is the size of pattern
 p is the number of processors
 distribute[] array to hold the number of values taken from file for each processor
description:
 calculates the number of bytes from the file each processor should recieve
 each processor get atleast file_size/p and the extra is allocated to all but the last
*/
void distribute_file(intmax_t file_size, size_t pattern_size, int p, int distribute[]) {
    int n = file_size/p;
    int r = file_size % p;
    for(int i = 0; i < p; i++){
        distribute[i] = n;
        if(r > 0) {
            distribute[i]++;
            r--;
        }
        if(i < p - 1) {
            distribute[i] += pattern_size - 1;
        }
    }
}


/* function to search file_buffer at specifed location and compare to pattern*/
void* find_string(void  *thread_data) {
    task_data *t_data;
    int   thread_id;
    int i = 0,j = 0;
    t_data    = (task_data*) thread_data;
    thread_id = t_data->task_id;
    for(i = t_data->first; i <= t_data->last; i++) {
        j = 0;
        while(j < pattern_size && i + j < file_size - 1) {
            if (file_buffer[i+j] != pattern[j]) {
                break;
            }
            j++;
        }
        /*if patter found lock the mutex and modify the found and count var to avoid race conditions*/
        if (j == pattern_size) {
            pthread_mutex_lock(&mutex);
            found[count].thread_id = thread_id;
            found[count].index = i;
            count++;
            pthread_mutex_unlock(&mutex);
        }
    }
    pthread_exit((void*) 0);
}



/* function used to convert command line to valid number*/
/* returns number or call fatal error to quit if arg is not valid*/
int arg_to_num(char *arg) {
    errno = 0;
    int arg_num = strtol(arg, NULL, 10);
    if(errno == ERANGE || arg_num < 0 || arg_num >= file_size) {
        fatal_error(errno, "Invalid number of threads");
    }
    return arg_num;
}

/* function used to compare the values in found*/
int compare(const void *p1, const void *p2) {
    x* x1 = (x*)p1;
    x* x2 = (x*)p2;
    if(x1 -> index == x2 -> index) {
        return x1 -> thread_id - x2 -> thread_id;
    }
    return x1 -> index - x2 -> index;
}

int main( int argc, char *argv[])
{
    int        retval;
    int        t;
    pthread_t  *threads;
    task_data  *thread_data;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if ( argc != 5 ) {
        usage_error("<num of threads> <pattern> <input file> <output file>");
    }
    
    /* read file */
    int fd; /* file descriptor*/

    //open the file
    fd = open(argv[3], O_RDONLY);
    if (fd == -1) {
        fatal_error(errno, "Unable to open file");
    }
    //get file size
    file_size = lseek(fd, 0, SEEK_END);
    if (file_size == -1) {
        fatal_error(errno, "Error seeking in file");
    }
    //if file is smaller than the pattern return error
    pattern = argv[2];
    pattern_size = strlen(pattern);
    if(file_size - 1 < pattern_size) {
        fatal_error(0, "pattern is larger than file");
    }
    //allocate buffer to read the file
    file_buffer = malloc(file_size);
    if (!file_buffer) {
        fatal_error(0, "Memory allocation failed for file buffer");
    }
    //read entire file into buffer
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
      fatal_error(errno, "Error seeking in file");
    }
    if (read(fd, file_buffer, file_size) != file_size) {
        fatal_error(errno, "Error reading from file");
    }
    //close file
    close(fd);
    
    //open output file and wrtie file_buffer to it
    char* file_out = argv[4];
    int in_fd = open(file_out, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (in_fd == -1) {
        fatal_error(errno, "Unable to open file");
    }
    lseek(in_fd, 0, SEEK_SET);  // Adjust position for output
    write(in_fd, file_buffer, file_size);  // Write redacted characters
    close(in_fd);
    
    /* convert num of threads to numbers*/
    num_threads = arg_to_num(argv[1]);
    
//    size_t max_matches = file_size - pattern_size - 2;
    found = malloc(250 * sizeof(x));
    if (!found) {
        fatal_error(0, "Failed to allocate memory for found matches");
    }
    
    /* distribute and displs array to hold the values*/
    int distribute[num_threads]; /* the amount of the file each processor gets*/
    int displs[num_threads]; /* the place from the file each processor gets*/
    
    /* get the distubtion and displacment arrays set*/
    distribute_file(file_size-1, pattern_size, num_threads, distribute);
    displacment(displs, distribute, pattern_size, num_threads);

    /* Allocate the array of threads, task_data structures, data and sums */
    threads     = calloc( num_threads, sizeof(pthread_t));
    thread_data = calloc( num_threads, sizeof(task_data));

    if(threads == NULL || thread_data == NULL)
        fatal_error(errno, "Failed to allocate memory for threads");

    /* count to hold the number of occurnces found*/
    count = 0;
    
    /* initilazie mutex*/
    pthread_mutex_init(&mutex, NULL);
    
    /* Initialize task_data for each thread and then create the thread */
    for(t = 0 ; t < num_threads; t++) {
        thread_data[t].first     = displs[t];
        thread_data[t].last      = distribute[t] + displs[t];
        if(thread_data[t].last >= file_size - 1) thread_data[t].last = file_size - 2;
        thread_data[t].task_id   = t;
//        printf("thread %d first: %d last: %d\n", thread_data[t].task_id, thread_data[t].first, thread_data[t].last);
        retval = pthread_create(&threads[t], &attr, find_string, (void *) &thread_data[t]);
        if ( retval ) {
            printf("ERROR; return code from pthread_create() is %d\n", retval);
            exit(-1);
        }  
    }

    /* wait for all threads to join main thread*/
    for(int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* destory mutex no longer needed*/
    pthread_mutex_destroy(&mutex);
    
    /* sort found to write to output file correctly*/
    qsort(found, count, sizeof(x), compare);

    /* allocate mem to hold the redacated char*/
    char *initialContent = malloc(pattern_size);
    if (!initialContent) {
        fatal_error(0, "Memory allocation failed for redacted char buffer");
    }
    
    /* open the output file to modify it according to findings*/
    in_fd = open(file_out, O_WRONLY, 0666);
    if (in_fd == -1) {
        fatal_error(errno, "Unable to open file");
    }
    for(int i = 0; i < count; i++) {
        memset(initialContent, REDACT_CHARS[found[i].thread_id % 64], pattern_size);
        if(lseek(in_fd, found[i].index, SEEK_SET) == (off_t)-1)
            fatal_error(errno, "Error seeking in file");
        if(write(in_fd, initialContent, pattern_size) != pattern_size)
           fatal_error(errno, "Error writing to file");
    }
    close(in_fd);

    free(threads);
    free(thread_data);
    free(initialContent);

    return 0;
}
