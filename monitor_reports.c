#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#define PID_FILE ".monitor_pid"

void handle_sigint(int sig){
    printf("Monitor stopping (SIGINT received)\n");
    unlink(PID_FILE);
    exit(0);
}

void handle_sigusr1(int sig){
    printf("New report added!\n");
}

int main(){
    int fd = open(PID_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if(fd < 0){
        perror("open");
        return 1;
    }

    char buf[50];
    sprintf(buf, "%d", getpid());
    write(fd, buf, strlen(buf));
    close(fd);

    struct sigaction sa1, sa2;

    sa1.sa_handler = handle_sigint;
    sigemptyset(&sa1.sa_mask);
    sa1.sa_flags = 0;
    sigaction(SIGINT, &sa1, NULL);

    sa2.sa_handler = handle_sigusr1;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0;
    sigaction(SIGUSR1, &sa2, NULL);

    while(1){
        pause();
    }

    return 0;
}