// Compile code:
// gcc -Wall -o <treasure_manager> treasure_manager.c
// Execute code:
// ./<treasure_manager> --<command> <hunt_id> (<treasure_id>)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>

#define MAX_PATH 256
#define MAX_USERNAME 64
#define MAX_CLUE 256
#define HUNT_DIR_PREFIX "./hunts/"  // Directory prefix for hunts

// Structure for a treasure record (fixed size)
typedef struct {
    int id;                        // Treasure ID
    char username[MAX_USERNAME];   // User name
    float latitude;                // GPS latitude
    float longitude;               // GPS longitude
    char clue[MAX_CLUE];           // Clue text
    int value;                     // Value of the treasure
    char is_active;                // 1 for active or 0 for deleted
} Treasure;

// Function prototypes
void add_treasure(const char *hunt_id);
void list_treasures(const char *hunt_id);
void view_treasure(const char *hunt_id, int treasure_id);
void remove_treasure(const char *hunt_id, int treasure_id);
void remove_hunt(const char *hunt_id);
void log_operation(const char *hunt_id, const char *operation);
void create_symlink(const char *hunt_id);
void ensure_hunt_directory(const char *hunt_id);
int get_next_treasure_id(const char *hunt_id);
char* get_treasure_file_path(const char *hunt_id);
char* get_log_file_path(const char *hunt_id);
void format_time(time_t time_value, char *buffer, size_t buffer_size);
void delete_file(const char *filepath);
void create_link(const char *target, const char *linkpath);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Format: treasure_manager --<command> [hunt_id] [treasure_id]\n");
        return 1;
    }

    // Parse command
    if (strcmp(argv[1], "--add") == 0) {
        if (argc < 3) {
            printf("Format: treasure_manager --add <hunt_id>\n");
            return 1;
        }
        add_treasure(argv[2]);
    } 
    else if (strcmp(argv[1], "--list") == 0) {
        if (argc < 3) {
            printf("Format: treasure_manager --list <hunt_id>\n");
            return 1;
        }
        list_treasures(argv[2]);
    } 
    else if (strcmp(argv[1], "--view") == 0) {
        if (argc < 4) {
            printf("Format: treasure_manager --view <hunt_id> <treasure_id>\n");
            return 1;
        }
        view_treasure(argv[2], atoi(argv[3]));
    } 
    else if (strcmp(argv[1], "--remove_treasure") == 0) {
        if (argc < 4) {
            printf("Format: treasure_manager --remove_treasure <hunt_id> <treasure_id>\n");
            return 1;
        }
        remove_treasure(argv[2], atoi(argv[3]));
    } 
    else if (strcmp(argv[1], "--remove_hunt") == 0) {
        if (argc < 3) {
            printf("Format: treasure_manager --remove_hunt <hunt_id>\n");
            return 1;
        }
        remove_hunt(argv[2]);
    } 
    else {
        printf("Unknown command: %s\n", argv[1]);
        return 1;
    }

    return 0;
}

void format_time(time_t time_value, char *buffer, size_t buffer_size) {
    struct tm *time_info;
    time_info = localtime(&time_value);
    
    // Format: YYYY-MM-DD HH:MM:SS
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
            time_info->tm_year + 1900,
            time_info->tm_mon + 1,
            time_info->tm_mday,
            time_info->tm_hour,
            time_info->tm_min,
            time_info->tm_sec);
}

void delete_file(const char *filepath) {
    if (remove(filepath) == -1 && errno != ENOENT) {
        perror("Failed to remove file");
    }
}

void create_link(const char *target, const char *linkpath) {
    // Try to remove existing link if it exists
    remove(linkpath);
    
    // Create the link using a system call to ln -s as an alternative
    char command[MAX_PATH * 2 + 10];
    strcpy(command, "ln -s ");
    strcat(command, target);
    strcat(command, " ");
    strcat(command, linkpath);
    
    if (system(command) != 0) {
        perror("Failed to create symbolic link");
    }
}

void ensure_hunt_directory(const char *hunt_id) {
    char hunt_path[MAX_PATH];
    
    // Create hunts directory if it doesn't exist
    mkdir("hunts", 0755);
    
    // Construct the hunt directory path
    strcpy(hunt_path, HUNT_DIR_PREFIX);
    strcat(hunt_path, hunt_id);
    
    // Create hunt directory if it doesn't exist
    if (mkdir(hunt_path, 0755) == -1) {
        if (errno != EEXIST) {
            perror("Failed to create hunt directory");
            exit(1);
        }
    }
}

// Get the path to the treasure file for a hunt
char* get_treasure_file_path(const char *hunt_id) {
    static char file_path[MAX_PATH];
    
    strcpy(file_path, HUNT_DIR_PREFIX);
    strcat(file_path, hunt_id);
    strcat(file_path, "/treasures.dat");
    
    return file_path;
}

// Get the path to the log file for a hunt
char* get_log_file_path(const char *hunt_id) {
    static char log_path[MAX_PATH];
    
    strcpy(log_path, HUNT_DIR_PREFIX);
    strcat(log_path, hunt_id);
    strcat(log_path, "/logged_hunt");
    
    return log_path;
}

// Create a symbolic link to the log file
void create_symlink(const char *hunt_id) {
    char log_path[MAX_PATH];
    char link_path[MAX_PATH] = "./logged_hunt-";
    
    strcpy(log_path, HUNT_DIR_PREFIX);
    strcat(log_path, hunt_id);
    strcat(log_path, "/logged_hunt");
    
    strcat(link_path, hunt_id);
    
    create_link(log_path, link_path);
}

// Log an operation to the hunt's log file
void log_operation(const char *hunt_id, const char *operation) {
    char *log_path = get_log_file_path(hunt_id);
    int log_fd;
    time_t now = time(NULL);
    char time_str[30];
    char log_entry[512];
    
    // Format the current time
    format_time(now, time_str, sizeof(time_str));
    
    // Format the log entry
    strcpy(log_entry, "[");
    strcat(log_entry, time_str);
    strcat(log_entry, "] ");
    strcat(log_entry, operation);
    strcat(log_entry, "\n");
    
    // Open log file in append mode, or create if it doesn't exist
    log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd == -1) {
        perror("Failed to open log file");
        return;
    }
    
    // Write log entry
    if (write(log_fd, log_entry, strlen(log_entry)) == -1) {
        perror("Failed to write to log file");
    }
    
    close(log_fd);
    
    // Create or update symbolic link
    create_symlink(hunt_id);
}

// Get the next available treasure ID
int get_next_treasure_id(const char *hunt_id) {
    char *file_path = get_treasure_file_path(hunt_id);
    int fd, max_id = 0;
    Treasure treasure;
    
    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        // If file doesn't exist, start with ID 1
        if (errno == ENOENT) {
            return 1;
        }
        perror("Failed to open treasure file");
        exit(1);
    }
    
    // Read all treasures to find the highest ID
    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (treasure.is_active && treasure.id > max_id) {
            max_id = treasure.id;
        }
    }
    
    close(fd);
    return max_id + 1;
}

// Add a new treasure to a hunt
void add_treasure(const char *hunt_id) {
    Treasure new_treasure;
    char *file_path;
    int fd;
    char log_message[256];
    
    // Ensure the hunt directory exists
    ensure_hunt_directory(hunt_id);
    
    // Get the file path
    file_path = get_treasure_file_path(hunt_id);
    
    // Get the next available ID
    new_treasure.id = get_next_treasure_id(hunt_id);
    new_treasure.is_active = 1;  // Mark as active
    
    // Get treasure details from user
    printf("Enter username (max %d chars): ", MAX_USERNAME - 1);
    fgets(new_treasure.username, MAX_USERNAME, stdin);
    new_treasure.username[strcspn(new_treasure.username, "\n")] = 0;  // Remove newline
    
    printf("Enter latitude: ");
    scanf("%f", &new_treasure.latitude);
    
    printf("Enter longitude: ");
    scanf("%f", &new_treasure.longitude);
    
    // Clear the newline from scanf
    getchar();
    
    printf("Enter clue (max %d chars): ", MAX_CLUE - 1);
    fgets(new_treasure.clue, MAX_CLUE, stdin);
    new_treasure.clue[strcspn(new_treasure.clue, "\n")] = 0;  // Remove newline
    
    printf("Enter value: ");
    scanf("%d", &new_treasure.value);
    
    // Open the file in append mode, create if it doesn't exist
    fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Failed to open treasure file");
        exit(1);
    }
    
    // Write the new treasure
    if (write(fd, &new_treasure, sizeof(Treasure)) != sizeof(Treasure)) {
        perror("Failed to write treasure");
        close(fd);
        exit(1);
    }
    
    close(fd);
    
    // Log the operation
    strcpy(log_message, "Added treasure ID ");
    char id_str[16];
    sprintf(id_str, "%d", new_treasure.id);
    strcat(log_message, id_str);
    strcat(log_message, " by ");
    strcat(log_message, new_treasure.username);
    
    log_operation(hunt_id, log_message);
    
    printf("Treasure added successfully with ID %d\n", new_treasure.id);
}

// List all treasures in a hunt
void list_treasures(const char *hunt_id) {
    char *file_path = get_treasure_file_path(hunt_id);
    int fd;
    Treasure treasure;
    struct stat file_stat;
    char time_str[30];
    char log_message[256];
    int count = 0;
    
    // Open the treasure file
    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        if (errno == ENOENT) {
            printf("Hunt '%s' has no treasures or does not exist.\n", hunt_id);
            return;
        }
        perror("Failed to open treasure file");
        exit(1);
    }
    
    // Get file stats
    if (fstat(fd, &file_stat) == -1) {
        perror("Failed to get file stats");
        close(fd);
        exit(1);
    }
    
    // Format the last modification time
    format_time(file_stat.st_mtime, time_str, sizeof(time_str));
    
    // Print hunt information
    printf("Hunt: %s\n", hunt_id);
    printf("Total file size: %ld bytes\n", (long)file_stat.st_size);
    printf("Last modified: %s\n\n", time_str);
    printf("Treasures:\n");
    printf("--------------------------------------------------\n");
    
    // Read and print all active treasures
    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (treasure.is_active) {
            printf("ID: %d | User: %s | Value: %d\n", 
                   treasure.id, treasure.username, treasure.value);
            count++;
        }
    }
    
    if (count == 0) {
        printf("No active treasures found in this hunt.\n");
    }
    
    printf("--------------------------------------------------\n");
    printf("Total treasures: %d\n", count);
    
    close(fd);
    
    // Log the operation
    strcpy(log_message, "Listed treasures for hunt '");
    strcat(log_message, hunt_id);
    strcat(log_message, "'");
    log_operation(hunt_id, log_message);
}

// View details of a specific treasure
void view_treasure(const char *hunt_id, int treasure_id) {
    char *file_path = get_treasure_file_path(hunt_id);
    int fd;
    Treasure treasure;
    int found = 0;
    char log_message[256];
    char id_str[16];
    
    // Open the treasure file
    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        if (errno == ENOENT) {
            printf("Hunt '%s' has no treasures or does not exist.\n", hunt_id);
            return;
        }
        perror("Failed to open treasure file");
        exit(1);
    }
    
    // Search for the treasure with the specified ID
    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (treasure.is_active && treasure.id == treasure_id) {
            found = 1;
            break;
        }
    }
    
    close(fd);
    
    if (found) {
        printf("Treasure Details:\n");
        printf("--------------------------------------------------\n");
        printf("ID: %d\n", treasure.id);
        printf("User: %s\n", treasure.username);
        printf("Location: %.6f, %.6f\n", treasure.latitude, treasure.longitude);
        printf("Clue: %s\n", treasure.clue);
        printf("Value: %d\n", treasure.value);
        printf("--------------------------------------------------\n");
        
        // Log the operation
        strcpy(log_message, "Viewed treasure ID ");
        sprintf(id_str, "%d", treasure_id);
        strcat(log_message, id_str);
        strcat(log_message, " from hunt '");
        strcat(log_message, hunt_id);
        strcat(log_message, "'");
        
        log_operation(hunt_id, log_message);
    } else {
        printf("Treasure with ID %d not found in hunt '%s'.\n", treasure_id, hunt_id);
    }
}

// Remove a treasure from a hunt
void remove_treasure(const char *hunt_id, int treasure_id) {
    char *file_path = get_treasure_file_path(hunt_id);
    int fd;
    Treasure treasure;
    off_t position;
    int found = 0;
    char log_message[256];
    char id_str[16];
    
    // Open the treasure file for reading and writing
    fd = open(file_path, O_RDWR);
    if (fd == -1) {
        if (errno == ENOENT) {
            printf("Hunt '%s' has no treasures or does not exist.\n", hunt_id);
            return;
        }
        perror("Failed to open treasure file");
        exit(1);
    }
    
    // Search for the treasure with the specified ID
    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        position = lseek(fd, 0, SEEK_CUR) - sizeof(Treasure);
        
        if (treasure.is_active && treasure.id == treasure_id) {
            found = 1;
            
            // Mark the treasure as inactive
            treasure.is_active = 0;
            
            // Move back to the position of this treasure
            if (lseek(fd, position, SEEK_SET) == -1) {
                perror("Failed to seek in file");
                close(fd);
                exit(1);
            }
            
            // Write the modified treasure
            if (write(fd, &treasure, sizeof(Treasure)) != sizeof(Treasure)) {
                perror("Failed to update treasure");
                close(fd);
                exit(1);
            }
            
            break;
        }
    }
    
    close(fd);
    
    if (found) {
        printf("Treasure with ID %d removed successfully.\n", treasure_id);
        
        // Log the operation
        strcpy(log_message, "Removed treasure ID ");
        sprintf(id_str, "%d", treasure_id);
        strcat(log_message, id_str);
        strcat(log_message, " from hunt '");
        strcat(log_message, hunt_id);
        strcat(log_message, "'");
        
        log_operation(hunt_id, log_message);
    } else {
        printf("Treasure with ID %d not found in hunt '%s'.\n", treasure_id, hunt_id);
    }
}

// Remove a hunt
void remove_hunt(const char *hunt_id) {
    char hunt_path[MAX_PATH];
    char treasure_file[MAX_PATH];
    char log_file[MAX_PATH];
    char symlink_path[MAX_PATH] = "./logged_hunt-";
    char log_message[256];
    
    // Construct paths
    strcpy(hunt_path, HUNT_DIR_PREFIX);
    strcat(hunt_path, hunt_id);
    
    strcpy(treasure_file, hunt_path);
    strcat(treasure_file, "/treasures.dat");
    
    strcpy(log_file, hunt_path);
    strcat(log_file, "/logged_hunt");
    
    strcat(symlink_path, hunt_id);
    
    // Log the operation before removing the hunt
    strcpy(log_message, "Removing hunt '");
    strcat(log_message, hunt_id);
    strcat(log_message, "'");
    log_operation(hunt_id, log_message);
    
    // Remove the treasure file
    delete_file(treasure_file);
    
    // Remove the log file
    delete_file(log_file);
    
    // Remove the symlink
    delete_file(symlink_path);
    
    // Remove the hunt directory
    if (rmdir(hunt_path) == -1) {
        if (errno == ENOENT) {
            printf("Hunt '%s' does not exist.\n", hunt_id);
            return;
        } else if (errno == ENOTEMPTY) {
            printf("Hunt directory is not empty. Some files may need to be removed manually.\n");
        } else {
            perror("Failed to remove hunt directory");
        }
    } else {
        printf("Hunt '%s' removed successfully.\n", hunt_id);
    }
}