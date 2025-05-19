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
#include <dirent.h>

#define MAX_CMD_LEN 256
#define MAX_BUFFER_SIZE 4096
#define COMMAND_FILE "monitor_command.txt"
#define PARAM_FILE "monitor_params.txt"
#define HUNTS_DIR "./hunts"

// Global variables
pid_t monitor_pid = -1;
volatile sig_atomic_t monitor_exiting = 0;
volatile sig_atomic_t child_exited = 0;
int exit_status = 0;
int pipe_fd[2]; // Pipe for monitor to send back results

// Function prototypes
void handle_sigchld(int sig);
void send_command_to_monitor(const char *command, const char *params);
void start_monitor();
void list_hunts();
void list_treasures(const char *hunt_id);
void view_treasure(const char *hunt_id, const char *treasure_id);
void stop_monitor();
void calculate_score();
void process_command(char *cmd);
void trim_newline(char *str);
void read_monitor_output();
void launch_score_calculator(const char *hunt_id);

// Signal handler for SIGCHLD
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
            
            // Close pipe when monitor exits
            close(pipe_fd[0]);
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
    
    /* Notify monitor about the new command */
    if (monitor_pid > 0) {
        kill(monitor_pid, SIGUSR1);
    }
}


void read_monitor_output() {
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_read;
    
    // Set pipe to non-blocking
    int flags = fcntl(pipe_fd[0], F_GETFL, 0);
    fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK);
    
    // Allow some time for data to arrive
    usleep(100000); // 0.1 seconds
    
    // Read from the pipe
    while ((bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
}


void start_monitor() {
    if (monitor_pid > 0) {
        printf("Monitor is already running (PID: %d)\n", monitor_pid);
        return;
    }
    
    // Create pipe for monitor to hub communication
    if (pipe(pipe_fd) == -1) {
        perror("Failed to create pipe");
        return;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
    } else if (pid == 0) {
        /* Child process - execute the monitor program */
        close(pipe_fd[0]); // Close read end
        
        // Duplicate the write end to stdout
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }
        
        close(pipe_fd[1]); // Close original write end
        
        // Pass pipe fd as argument to monitor
        char pipe_fd_str[16];
        snprintf(pipe_fd_str, sizeof(pipe_fd_str), "%d", pipe_fd[1]);
        
        execl("./treasure_monitor", "treasure_monitor", pipe_fd_str, NULL);
        perror("Exec failed");
        exit(EXIT_FAILURE);
    } else {
        /* Parent process */
        close(pipe_fd[1]); // Close write end
        monitor_pid = pid;
        printf("Monitor started with PID: %d\n", monitor_pid);
    }
}


// Send list_hunts command to the monitor
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
    read_monitor_output();
}


// Send list_treasures command to the monitor
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
    read_monitor_output();
}

// Send view_treasure command to the monitor
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
    read_monitor_output();
}


// Send stop command to the monitor
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
    read_monitor_output();
    printf("Stopping monitor...\n");
}


void launch_score_calculator(const char *hunt_id) {
    int score_pipe[2];
    pid_t score_pid;
    
    if (pipe(score_pipe) == -1) {
        perror("Failed to create score pipe");
        return;
    }
    
    score_pid = fork();
    if (score_pid < 0) {
        perror("Score calculator fork failed");
        close(score_pipe[0]);
        close(score_pipe[1]);
    } else if (score_pid == 0) {
        /* Child process - execute the score calculator */
        close(score_pipe[0]); // Close read end
        
        // Redirect stdout to the pipe
        if (dup2(score_pipe[1], STDOUT_FILENO) == -1) {
            perror("Score calculator dup2 failed");
            exit(EXIT_FAILURE);
        }
        
        close(score_pipe[1]); // Close original write end
        
        // Execute the score calculator with hunt_id as parameter
        execl("./score_calculator", "score_calculator", hunt_id, NULL);
        perror("Score calculator exec failed");
        exit(EXIT_FAILURE);
    } else {
        /* Parent process */
        close(score_pipe[1]); // Close write end
        
        // Read from the pipe
        char buffer[MAX_BUFFER_SIZE];
        ssize_t bytes_read;
        
        // Read and output the score results
        bytes_read = read(score_pipe[0], buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Scores for hunt '%s':\n", hunt_id);
            printf("%s", buffer);
        }
        
        close(score_pipe[0]);
        
        // Wait for the score calculator to finish
        int status;
        waitpid(score_pid, &status, 0);
    }
}


void calculate_score() {
    DIR *dir;
    struct dirent *entry;
    char hunt_id[MAX_CMD_LEN];
    
    dir = opendir(HUNTS_DIR);
    if (!dir) {
        perror("Failed to open hunts directory");
        return;
    }
    
    printf("Calculating scores for all hunts...\n");
    
    // For each hunt directory, launch a score calculator
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Launch score calculator for this hunt
        strcpy(hunt_id, entry->d_name);
        launch_score_calculator(hunt_id);
    }
    
    closedir(dir);
    printf("Score calculation complete.\n");
}

/**
 * Remove trailing newline from a string
 */
void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

/**
 * Process a command entered by the user
 */
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
    } else if (strcmp(token, "calculate_score") == 0) {
        calculate_score();
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
        printf("Available commands: start_monitor, list_hunts, list_treasures, view_treasure, calculate_score, stop_monitor, exit\n");
    }
}


int main() {
    char cmd[MAX_CMD_LEN];
    struct sigaction sa;
    
    /* Set up signal handler for SIGCHLD */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigchld;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
    
    printf("Treasure Hunt Hub\n");
    printf("=================\n");
    printf("Type 'start_monitor' to begin\n");
    
    while (1) {
        /* Check if monitor has exited */
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
