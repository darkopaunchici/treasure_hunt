#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 1024
#define MAX_USERS 100
#define MAX_ITEMS 100

typedef struct {
    char name[128];
    int value;
    char owner[128];
} Item;

typedef struct {
    char name[128];
    int score;
} User;

// Reads hunt data from stdin and calculates scores for each user
void calculate_scores() {
    char line[MAX_LINE];
    Item items[MAX_ITEMS];
    User users[MAX_USERS];
    int item_count = 0;
    int user_count = 0;
    
    // Read items data from stdin
    while (fgets(line, MAX_LINE, stdin) != NULL) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines
        if (strlen(line) == 0) {
            continue;
        }
        
        // Parse item data (expected format: "name,value,owner")
        char *token = strtok(line, ",");
        if (token == NULL) continue;
        strcpy(items[item_count].name, token);
        
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        items[item_count].value = atoi(token);
        
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        strcpy(items[item_count].owner, token);
        
        // Record unique users
        int user_exists = 0;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].name, items[item_count].owner) == 0) {
                user_exists = 1;
                break;
            }
        }
        
        if (!user_exists && strcmp(items[item_count].owner, "none") != 0) {
            strcpy(users[user_count].name, items[item_count].owner);
            users[user_count].score = 0;
            user_count++;
        }
        
        item_count++;
        if (item_count >= MAX_ITEMS) break;
    }
    
    // Calculate scores for each user
    for (int i = 0; i < user_count; i++) {
        for (int j = 0; j < item_count; j++) {
            if (strcmp(items[j].owner, users[i].name) == 0) {
                users[i].score += items[j].value;
            }
        }
    }
    
    // Output scores
    printf("===== USER SCORES =====\n");
    for (int i = 0; i < user_count; i++) {
        printf("%s: %d points\n", users[i].name, users[i].score);
    }
    
    // If no users found
    if (user_count == 0) {
        printf("No users with items found in this hunt.\n");
    }
}

int main(void) {
    calculate_scores();
    return 0;
}
