#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#define PID_FILE ".monitor_pid"

void handle_sigint(int sig){
    const char *msg = "[Monitor Process Signaling] System stopping gracefully (SIGINT received).\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    unlink(PID_FILE);
    _exit(0);
}

void handle_sigusr1(int sig){
    const char *msg = "[Monitor Alert System] Async Event Captured: A new infrastructure report was logged!\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

int main(){
    //PHASE e REQUIREMENT: check if mnitor is already running
    int check_fd= open(PID_FILE, O_RDONLY);
    if(check_fd>=0){
        char old_pid_buff[64]={0};
        int bytes_read=read(check_fd, old_pid_buff, sizeof(old_pid_buff)-1);
        close(check_fd);

        if(bytes_read>0){
            pid_t old_pid=atoi(old_pid_buff);
            if(kill(old_pid,0)==0){
                printf("ERROR: already_running:%d\n", old_pid);
                fflush(stdout);
                return 1;
            }
        }
    }
    int fd = open(PID_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if(fd < 0){
        perror("Fatal error generating core background process control file PID_FILE");
        return 1;
    }

    char buf[50];
    snprintf(buf, sizeof(buf), "%d\n", getpid());
    if (write(fd, buf, strlen(buf)) < 0) {
        perror("Error writing to .monitor_pid");
        close(fd);
        return 1;
    }
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

    printf("STATUS:RUNNING:%d\n", getpid());
    fflush(stdout);

    while(1){
        pause();
    }

    return 0;
}