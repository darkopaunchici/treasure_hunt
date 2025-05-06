#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_CMD_LEN 256
#define COMMAND_FILE "monitor_command.txt"
#define PARAM_FILE "monitor_params.txt"

// Global variables
pid_t monitor_pid = -1;
volatile sig_atomic_t monitor_exiting = 0;
volatile sig_atomic_t child_exited = 0;
int exit_status = 0;

// Function prototypes
void handle_sigchld(int sig);
void send_command_to_monitor(const char *command, const char *params);
void start_monitor();
void list_hunts();
void list_treasures(const char *hunt_id);
void view_treasure(const char *hunt_id, const char *treasure_id);
void stop_monitor();
void process_command(char *cmd);
void trim_newline(char *str);


void handle_sigchld() {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == monitor_pid) {
            child_exited = 1;
            if (WIFEXITED(status)) {
                exit_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                exit_status = 128 + WTERMSIG(status);
            }
            monitor_pid = -1;
        }
    }
}


void send_command_to_monitor(const char *command, const char *params) {
    FILE *cmd_file = fopen(COMMAND_FILE, "w");
    if (cmd_file) {
        fprintf(cmd_file, "%s", command);
        fclose(cmd_file);
    } else {
        perror("Failed to write command file");
    }
    
    if (params) {
        FILE *param_file = fopen(PARAM_FILE, "w");
        if (param_file) {
            fprintf(param_file, "%s", params);
            fclose(param_file);
        } else {
            perror("Failed to write params file");
        }
    }
    
    // Notify monitor about the new command
    if (monitor_pid > 0) {
        kill(monitor_pid, SIGUSR1);
    }
}


void start_monitor() {
    if (monitor_pid > 0) {
        printf("Monitor is already running (PID: %d)\n", monitor_pid);
        return;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
    } else if (pid == 0) {
        /* Child process - execute the monitor program */
        execl("./treasure_monitor", "treasure_monitor", NULL);
        perror("Exec failed");
        exit(EXIT_FAILURE);
    } else {
        /* Parent process */
        monitor_pid = pid;
        printf("Monitor started with PID: %d\n", monitor_pid);
    }
}


void list_hunts() {
    if (monitor_pid < 0) {
        printf("Error: Monitor is not running\n");
        return;
    }
    
    if (monitor_exiting) {
        printf("Error: Monitor is in the process of exiting\n");
        return;
    }
    
    send_command_to_monitor("list_hunts", NULL);
}


void list_treasures(const char *hunt_id) {
    if (monitor_pid < 0) {
        printf("Error: Monitor is not running\n");
        return;
    }
    
    if (monitor_exiting) {
        printf("Error: Monitor is in the process of exiting\n");
        return;
    }
    
    send_command_to_monitor("list_treasures", hunt_id);
}


void view_treasure(const char *hunt_id, const char *treasure_id) {
    if (monitor_pid < 0) {
        printf("Error: Monitor is not running\n");
        return;
    }
    
    if (monitor_exiting) {
        printf("Error: Monitor is in the process of exiting\n");
        return;
    }
    
    char params[MAX_CMD_LEN];
    snprintf(params, sizeof(params), "%s %s", hunt_id, treasure_id);
    send_command_to_monitor("view_treasure", params);
}


void stop_monitor() {
    if (monitor_pid < 0) {
        printf("Error: Monitor is not running\n");
        return;
    }
    
    if (monitor_exiting) {
        printf("Error: Monitor is already in the process of exiting\n");
        return;
    }
    
    monitor_exiting = 1;
    send_command_to_monitor("stop", NULL);
    printf("Stopping monitor...\n");
}

void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}


//Process a command entered by the user
void process_command(char *cmd) {
    char *token;
    
    trim_newline(cmd);
    
    token = strtok(cmd, " ");
    if (!token) return;
    
    if (strcmp(token, "start_monitor") == 0) {
        start_monitor();
    } else if (strcmp(token, "list_hunts") == 0) {
        list_hunts();
    } else if (strcmp(token, "list_treasures") == 0) {
        token = strtok(NULL, " ");
        if (token) {
            list_treasures(token);
        } else {
            printf("Error: Missing hunt ID\n");
        }
    } else if (strcmp(token, "view_treasure") == 0) {
        char *hunt_id = strtok(NULL, " ");
        char *treasure_id = strtok(NULL, " ");
        if (hunt_id && treasure_id) {
            view_treasure(hunt_id, treasure_id);
        } else {
            printf("Error: Missing hunt ID or treasure ID\n");
        }
    } else if (strcmp(token, "stop_monitor") == 0) {
        stop_monitor();
    } else if (strcmp(token, "exit") == 0) {
        if (monitor_pid > 0) {
            printf("Error: Monitor is still running. Stop it first with 'stop_monitor'\n");
        } else {
            printf("Exiting treasure_hub\n");
            exit(EXIT_SUCCESS);
        }
    } else {
        printf("Unknown command: %s\n", token);
        printf("Available commands: start_monitor, list_hunts, list_treasures, view_treasure, stop_monitor, exit\n");
    }
}

int main() {
    char cmd[MAX_CMD_LEN];
    struct sigaction sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigchld;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
    
    printf("Treasure Hunt Hub\n");
    printf("=================\n");
    printf("Type 'start_monitor' to begin\n");
    
    while (1) {
        // Check if monitor has exited
        if (child_exited) {
            printf("Monitor has terminated with status %d\n", exit_status);
            child_exited = 0;
            monitor_exiting = 0;
        }
        
        printf("> ");
        if (fgets(cmd, MAX_CMD_LEN, stdin) == NULL) {
            if (feof(stdin)) {
                printf("\nEnd of input. Exiting.\n");
                break;
            }
            perror("Error reading command");
            continue;
        }
        
        process_command(cmd);
    }
    
    return 0;
}
