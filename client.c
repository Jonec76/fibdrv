#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define BILLION 1000000000L

int main()
{
    char buf[128];
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        long long sz;
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }
    // FILE * fp;
    // fp = fopen ("fast_ker_user.txt", "w+");

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        // unsigned long long int diff;
        // unsigned long long int end_time;
        // struct timespec start, end;
        // clock_gettime(CLOCK_MONOTONIC, &start); // user
        read(fd, buf, 1);
        // clock_gettime(CLOCK_MONOTONIC, &end); // user, ker_user
        // end_time = BILLION * (end.tv_sec ) + end.tv_nsec ; // user, ker_user
        // diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec -
        // start.tv_nsec; // user fprintf(fp, "%lld\n", diff); // user
        // fprintf(fp, "%lld\n", sz); // kernel
        // fprintf(fp, "%lld\n", end_time - sz ); // ker_user
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }
    // fclose(fp);
    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    close(fd);
    return 0;
}
