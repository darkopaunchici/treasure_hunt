#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_CMD_LEN 256
#define COMMAND_FILE "monitor_command.txt"
#define PARAM_FILE "monitor_params.txt"
#define DELAY_BEFORE_EXIT 2000000  // 2 seconds in microseconds

// Global variables
volatile sig_atomic_t should_exit = 0;
volatile sig_atomic_t received_command = 0;

// Function prototypes
void handle_sigusr1(int sig);
void handle_command();
void execute_treasure_manager(const char *action, const char *params);
void list_hunts();
void list_treasures(const char *hunt_id);
void view_treasure(const char *hunt_id, const char *treasure_id);


void handle_sigusr1() {
    received_command = 1;
}


void handle_command() {
    char command[MAX_CMD_LEN] = {0};
    char params[MAX_CMD_LEN] = {0};
    FILE *cmd_file, *param_file;
    
    /* Read command */
    cmd_file = fopen(COMMAND_FILE, "r");
    if (cmd_file) {
        if (fgets(command, MAX_CMD_LEN, cmd_file)) {
            /* Remove newline character if present */
            size_t len = strlen(command);
            if (len > 0 && command[len - 1] == '\n') {
                command[len - 1] = '\0';
            }
        }
        fclose(cmd_file);
    } else {
        perror("Failed to open command file");
        return;
    }
    
    /* Read parameters if they exist */
    param_file = fopen(PARAM_FILE, "r");
    if (param_file) {
        if (fgets(params, MAX_CMD_LEN, param_file)) {
            /* Remove newline character if present */
            size_t len = strlen(params);
            if (len > 0 && params[len - 1] == '\n') {
                params[len - 1] = '\0';
            }
        }
        fclose(param_file);
    }
    
    /* Process the command */
    if (strcmp(command, "list_hunts") == 0) {
        list_hunts();
    } else if (strcmp(command, "list_treasures") == 0) {
        list_treasures(params);
    } else if (strcmp(command, "view_treasure") == 0) {
        char hunt_id[MAX_CMD_LEN] = {0};
        char treasure_id[MAX_CMD_LEN] = {0};
        
        /* Parse parameters */
        sscanf(params, "%s %s", hunt_id, treasure_id);
        view_treasure(hunt_id, treasure_id);
    } else if (strcmp(command, "stop") == 0) {
        printf("Monitor received stop command. Preparing to exit...\n");
        should_exit = 1;
    } else {
        printf("Monitor: Unknown command '%s'\n", command);
    }
}

void execute_treasure_manager(const char *action, const char *params) {
    pid_t pid;
    int status;
    
    pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return;
    } else if (pid == 0) {
        /* Child process */
        if (params) {
            execlp("./treasure_manager", "treasure_manager", action, params, NULL);
        } else {
            execlp("./treasure_manager", "treasure_manager", action, NULL);
        }
        perror("Exec failed");
        exit(EXIT_FAILURE);
    } else {
        /* Parent process - wait for the child to complete */
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            printf("Treasure manager exited with status %d\n", WEXITSTATUS(status));
        }
    }
}


void list_hunts() {
    printf("Monitor: Listing all hunts\n");
    execute_treasure_manager("list", NULL);
}


void list_treasures(const char *hunt_id) {
    printf("Monitor: Listing treasures for hunt %s\n", hunt_id);
    execute_treasure_manager("show", hunt_id);
}


void view_treasure(const char *hunt_id, const char *treasure_id) {
    char params[MAX_CMD_LEN];
    
    printf("Monitor: Viewing treasure %s in hunt %s\n", treasure_id, hunt_id);
    snprintf(params, sizeof(params), "%s %s", hunt_id, treasure_id);
    execute_treasure_manager("view", params);
}


int main() {
    struct sigaction sa;
    
    /* Set up signal handler for SIGUSR1 */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigusr1;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);
    
    printf("Treasure Monitor started (PID: %d)\n", getpid());
    
    /* Main loop */
    while (!should_exit) {
        if (received_command) {
            handle_command();
            received_command = 0;
        }
        
        /* Sleep briefly to avoid busy waiting */
        usleep(100000);  /* Sleep for 0.1 seconds */
    }
    
    /* Delay before actually exiting */
    printf("Monitor: Delaying before exit...\n");
    usleep(DELAY_BEFORE_EXIT);
    printf("Monitor: Exiting now\n");
    
    return 0;
}
