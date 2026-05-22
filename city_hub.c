#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_LINE 1024

// Data tracking structure for infrastructure report layout to process scores
typedef struct {
    int id;                // 4 bytes
    int severity;          // 4 bytes
    float latitude;        // 4 bytes
    float longitude;       // 4 bytes
    time_t timestamp;      // 8 bytes
    char inspector[52];    // 52 bytes
    char category[24];     // 24 bytes
    char description[108];
} ReportRecord;

/* ================= MONITOR PIPING SYSTEM ================= */
void launch_monitor_pipeline() {

    pid_t hub_mon_pid = fork();

    if(hub_mon_pid < 0){
        perror("fork");
        return;
    }

    /*
        PARENT = city_hub shell
        Immediately return to prompt
    */
    if(hub_mon_pid > 0){
        printf("Background hub_mon started with PID %d\n", hub_mon_pid);
        return;
    }

    /*
        CHILD = hub_mon
    */

    int pipe_fd[2];

    if(pipe(pipe_fd) < 0){
        perror("pipe");
        exit(1);
    }

    pid_t monitor_pid = fork();

    if(monitor_pid < 0){
        perror("fork");
        exit(1);
    }

    /*
        MONITOR PROCESS
    */
   if (monitor_pid == 0) {
        close(pipe_fd[0]); // Close unused read side

        // Duplicate write end of pipe onto standard output descriptor
        if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
            perror("dup2 failed");
            exit(1);
        }
        close(pipe_fd[1]);

        // Execute the standalone compiled monitor program 
        // Note: Make sure to compile your monitor file using: gcc monitor_reports.c -o monitor_reports
        execl("./monitor_reports", "./monitor_reports", NULL);

        // If execl returns, an error definitely occurred
        perror("[ERROR] Execution of ./monitor_reports executable binary failed");
        exit(1);
    }

    /*
        hub_mon PROCESS
    */

    close(pipe_fd[1]);

    FILE *stream = fdopen(pipe_fd[0], "r");

    if(!stream){
        perror("fdopen");
        exit(1);
    }

    char buffer[MAX_LINE];

    while(fgets(buffer, sizeof(buffer), stream) != NULL){

        buffer[strcspn(buffer, "\n")] = '\0';

        /*
            STANDARDIZED PROTOCOL
        */

        if(strncmp(buffer, "STATUS:RUNNING:", 15) == 0){

            printf("[hub_mon] Monitor started -> %s\n", buffer + 15);
        }

        else if(strncmp(buffer, "EVENT:", 6) == 0){

            printf("[hub_mon] %s\n", buffer + 6);
        }

        else if(strncmp(buffer, "ERROR:ALREADY_RUNNING:", 22) == 0){

            printf("[hub_mon] Monitor already running with PID %s\n",
                   buffer + 22);

            break;
        }

        else if(strcmp(buffer, "STATUS:STOPPED") == 0){

            printf("[hub_mon] Monitor terminated\n");

            break;
        }

        else{

           printf("[hub_mon] Incoming Buffer Frame: %s\n", buffer);
        }
        fflush(stdout);
    }

    fclose(stream);

    waitpid(monitor_pid, NULL, 0);

    printf("[hub_mon] Background monitoring pipeline processor exiting.\n");
    exit(0);
}

/* ================= SCORER CALCULATION SYSTEM ================= */
void calculate_scores(int district_count, char *districts[]) {
    int total_pipes = district_count;
    int pipes[total_pipes][2];
    pid_t child_pids[total_pipes];

    // Initialize all IPC plumbing channels upfront
    for (int i = 0; i < district_count; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("IPC pipeline error setup");
            return;
        }
    }

    // Spawn concurrent computing processes
    for (int i = 0; i < district_count; i++) {
        child_pids[i] = fork();
        if (child_pids[i] < 0) {
            perror("Fork failed mapping workload process execution");
            return;
        }

        if (child_pids[i] == 0) {
            // SCORER CHILD PROCESS CONTEXT
            // Wire standard output into the write side of the target processing segment
            dup2(pipes[i][1], STDOUT_FILENO);

            // Clean up unused file descriptors inside the worker context block
            for (int j = 0; j < district_count; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            char path[512];
            snprintf(path, sizeof(path), "%s/reports.dat", districts[i]);

            int fd = open(path, O_RDONLY);
            if (fd < 0) {
                // If the district path doesn't exist or file is empty, send cleanly formatted report back
                printf("District: %s -> No infrastructure database found or empty file paths\n", districts[i]);
                exit(0);
            }

            // Internal compute implementation directly inside C binary for maximized execution speed
            ReportRecord rec;
            typedef struct {
                char name[50];
                int total_score;
            } ScoreTrack;
            
            ScoreTrack tracker[128];
            int tracker_count = 0;

            while (read(fd, &rec, sizeof(ReportRecord)) > 0) {
                int found_idx = -1;
                for (int k = 0; k < tracker_count; k++) {
                    if (strcmp(tracker[k].name, rec.inspector) == 0) {
                        found_idx = k;
                        break;
                    }
                }
                if (found_idx != -1) {
                    tracker[found_idx].total_score += rec.severity;
                } else {
                    strncpy(tracker[tracker_count].name, rec.inspector, 49);
                    tracker[tracker_count].total_score = rec.severity;
                    tracker_count++;
                }
            }
            close(fd);

            printf("=========================================\n");
            printf(" DISTRICT WORKLOAD SUMMARY: %s\n", districts[i]);
            printf("=========================================\n");
            if(tracker_count == 0) {
                printf(" No entries processed inside file logs.\n");
            } else {
                for (int k = 0; k < tracker_count; k++) {
                    printf("  Inspector: %-15s | Total Workload Severity Score: %d\n", tracker[k].name, tracker[k].total_score);
                }
            }
            exit(0);
        }
    }

    // Interactive Hub Shell Parent Execution Path Block
    // Close worker side pipes to avoid thread deadlocking
    for (int i = 0; i < district_count; i++) {
        close(pipes[i][1]);
    }

    printf("\n Aggregating Distributed Workload Intelligence Scorecard Metrics:\n");
    
    // Harvest the calculated metrics across distinct file pipe pipelines sequentially
    for (int i = 0; i < district_count; i++) {
        char read_line[MAX_LINE];
        FILE *fp = fdopen(pipes[i][0], "r");
        if (fp) {
            while (fgets(read_line, sizeof(read_line), fp) != NULL) {
                printf("%s", read_line);
            }
            fclose(fp);
        }
        waitpid(child_pids[i], NULL, 0); // Reclaim process contexts cleanly
    }
    printf("\n");
}

/* ================= INTERACTIVE LOOP INTERFACE ================= */
int main() {
    char input_line[MAX_LINE];

    printf("=========================================================\n");
    printf("   CITY INFRASTRUCTURE CORE PIPELINE PLATFORM ENGINE 3.0 \n");
    printf("=========================================================\n");
    printf(" Commands: start_monitor | calculate_scores [districts...] | exit\n\n");

    while (1) {
        printf("city_hub> ");
        fflush(stdout);

        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            break;
        }

        // Clean trailing tokens
        input_line[strcspn(input_line, "\n")] = 0;

        if (strlen(input_line) == 0) continue;

        // Parse instruction verbs
        char *args[64];
        int arg_count = 0;
        char *token = strtok(input_line, " ");
        while (token != NULL && arg_count < 63) {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL;

        if (strcmp(args[0], "exit") == 0) {
            printf("Shutting down administrative control matrix environment console panel.\n");
            break;
        }
        else if (strcmp(args[0], "start_monitor") == 0) {
            // Spawns tracking engine via custom background descriptors logic
            launch_monitor_pipeline();
        }
        else if (strcmp(args[0], "calculate_scores") == 0) {
            if (arg_count < 2) {
                printf("Error: Command parameter requirements missing. Format: calculate_scores <downtown> [suburbs...]\n");
            } else {
                calculate_scores(arg_count - 1, &args[1]);
            }
        }
        else {
            printf("Error: Unrecognized execution keyword command option. Try again.\n");
        }
    }

    return 0;
}