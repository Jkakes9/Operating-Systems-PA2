#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>

#include "hash_table.h"

#define MAX_COMMANDS 1024
#define LOG_FILE "hash.log"
#define COMMAND_FILE "commands.txt"

typedef enum {
    CMD_INSERT,
    CMD_DELETE,
    CMD_UPDATE,
    CMD_SEARCH,
    CMD_PRINT
} command_type;

typedef struct {
    command_type type;
    char name[NAME_SIZE];
    uint32_t salary;
    int priority;
    int has_priority;
} command;

static pthread_rwlock_t list_lock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t order_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t order_cond = PTHREAD_COND_INITIALIZER;
static int current_turn = 0;
static FILE *log_file = NULL;

static long long current_timestamp(void) {
    struct timeval te;
    gettimeofday(&te, NULL);
    return (long long)(te.tv_sec * 1000000LL + te.tv_usec);
}

static void log_line(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        vfprintf(log_file, fmt, args);
        fflush(log_file);
    }
    pthread_mutex_unlock(&log_mutex);
    va_end(args);
}

static uint32_t jenkins_hash(const char *key) {
    size_t len = strlen(key);
    uint32_t hash = 0;
    for (size_t i = 0; i < len; i++) {
        hash += (unsigned char)key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

static void wait_for_turn(int priority) {
    pthread_mutex_lock(&order_mutex);
    log_line("%lld: THREAD %d WAITING FOR MY TURN\n", current_timestamp(), priority);
    while (priority != current_turn) {
        pthread_cond_wait(&order_cond, &order_mutex);
    }
    log_line("%lld: THREAD %d AWAKENED FOR WORK\n", current_timestamp(), priority);
    current_turn++;
    pthread_cond_broadcast(&order_cond);
    pthread_mutex_unlock(&order_mutex);
}

static void log_lock(int priority, const char *message) {
    log_line("%lld: THREAD %d %s\n", current_timestamp(), priority, message);
}

static void log_command(command *cmd, uint32_t hash_value) {
    switch (cmd->type) {
        case CMD_INSERT:
            log_line("%lld: THREAD %d INSERT,%u,%s,%u\n", current_timestamp(), cmd->priority, hash_value, cmd->name, cmd->salary);
            break;
        case CMD_DELETE:
            log_line("%lld: THREAD %d DELETE,%u,%s\n", current_timestamp(), cmd->priority, hash_value, cmd->name);
            break;
        case CMD_UPDATE:
            log_line("%lld: THREAD %d UPDATE,%u,%s,%u\n", current_timestamp(), cmd->priority, hash_value, cmd->name, cmd->salary);
            break;
        case CMD_SEARCH:
            log_line("%lld: THREAD %d SEARCH,%u,%s\n", current_timestamp(), cmd->priority, hash_value, cmd->name);
            break;
        case CMD_PRINT:
            log_line("%lld: THREAD %d PRINT\n", current_timestamp(), cmd->priority);
            break;
    }
}

static void *execute_command(void *arg) {
    command *cmd = (command *)arg;
    wait_for_turn(cmd->priority);

    uint32_t hash_value = 0;
    if (cmd->type != CMD_PRINT) {
        hash_value = jenkins_hash(cmd->name);
    }

    log_command(cmd, hash_value);

    switch (cmd->type) {
        case CMD_INSERT: {
            pthread_rwlock_wrlock(&list_lock);
            log_lock(cmd->priority, "WRITE LOCK ACQUIRED");
            bool inserted = ht_insert(hash_value, cmd->name, cmd->salary);
            pthread_rwlock_unlock(&list_lock);
            log_lock(cmd->priority, "WRITE LOCK RELEASED");
            if (inserted) {
                printf("Inserted %u,%s,%u\n", hash_value, cmd->name, cmd->salary);
            } else {
                printf("Insert failed. Entry %u is a duplicate.\n", hash_value);
            }
            break;
        }
        case CMD_DELETE: {
            pthread_rwlock_wrlock(&list_lock);
            log_lock(cmd->priority, "WRITE LOCK ACQUIRED");
            char removed_name[NAME_SIZE];
            uint32_t removed_salary = 0;
            bool removed = ht_delete(hash_value, removed_name, &removed_salary);
            pthread_rwlock_unlock(&list_lock);
            log_lock(cmd->priority, "WRITE LOCK RELEASED");
            if (removed) {
                printf("Deleted record for %u,%s,%u\n", hash_value, removed_name, removed_salary);
            } else {
                printf("Entry %u not deleted. Not in database.\n", hash_value);
            }
            break;
        }
        case CMD_UPDATE: {
            pthread_rwlock_wrlock(&list_lock);
            log_lock(cmd->priority, "WRITE LOCK ACQUIRED");
            uint32_t old_salary = 0;
            char name_out[NAME_SIZE];
            bool updated = ht_update(hash_value, cmd->salary, &old_salary, name_out);
            pthread_rwlock_unlock(&list_lock);
            log_lock(cmd->priority, "WRITE LOCK RELEASED");
            if (updated) {
                printf("Updated record %u from %u,%s,%u to %u,%s,%u\n", hash_value, hash_value, name_out, old_salary, hash_value, name_out, cmd->salary);
            } else {
                printf("Update failed. Entry %u not found.\n", hash_value);
            }
            break;
        }
        case CMD_SEARCH: {
            pthread_rwlock_rdlock(&list_lock);
            log_lock(cmd->priority, "READ LOCK ACQUIRED");
            char name_out[NAME_SIZE];
            uint32_t salary_out = 0;
            bool found = ht_search(hash_value, name_out, &salary_out);
            pthread_rwlock_unlock(&list_lock);
            log_lock(cmd->priority, "READ LOCK RELEASED");
            if (found) {
                printf("Found: %u,%s,%u\n", hash_value, name_out, salary_out);
            } else {
                printf("Not Found: %s not found.\n", cmd->name);
            }
            break;
        }
        case CMD_PRINT: {
            pthread_rwlock_rdlock(&list_lock);
            log_lock(cmd->priority, "READ LOCK ACQUIRED");
            printf("Current Database:\n");
            ht_print(stdout);
            pthread_rwlock_unlock(&list_lock);
            log_lock(cmd->priority, "READ LOCK RELEASED");
            break;
        }
    }

    free(cmd);
    return NULL;
}

static int parse_command_line(char *line, command *cmd, int *default_priority) {
    char *tokens[8];
    int count = 0;
    char *save = NULL;
    char *token = strtok_r(line, ",\n", &save);
    while (token && count < 8) {
        tokens[count++] = token;
        token = strtok_r(NULL, ",\n", &save);
    }
    if (count == 0) {
        return -1;
    }

    for (char *p = tokens[0]; *p; ++p) {
        *p = tolower(*p);
    }

    if (strcmp(tokens[0], "threads") == 0) {
        if (count >= 2) {
            int start_priority = (count >= 3) ? atoi(tokens[2]) : atoi(tokens[1]);
            current_turn = start_priority;
            *default_priority = start_priority;
        }
        return 1; // header line, no command to dispatch
    }

    if (strcmp(tokens[0], "insert") == 0) {
        if (count < 3) return -1;
        cmd->type = CMD_INSERT;
        strncpy(cmd->name, tokens[1], NAME_SIZE - 1);
        cmd->name[NAME_SIZE - 1] = '\0';
        cmd->salary = (uint32_t)strtoul(tokens[2], NULL, 10);
        if (count >= 4) {
            cmd->priority = atoi(tokens[count - 1]);
            cmd->has_priority = 1;
        } else {
            cmd->priority = (*default_priority)++;
            cmd->has_priority = 0;
        }
    } else if (strcmp(tokens[0], "delete") == 0) {
        if (count < 2) return -1;
        cmd->type = CMD_DELETE;
        strncpy(cmd->name, tokens[1], NAME_SIZE - 1);
        cmd->name[NAME_SIZE - 1] = '\0';
        if (count >= 3) {
            cmd->priority = atoi(tokens[count - 1]);
            cmd->has_priority = 1;
        } else {
            cmd->priority = (*default_priority)++;
            cmd->has_priority = 0;
        }
    } else if (strcmp(tokens[0], "update") == 0) {
        if (count < 3) return -1;
        cmd->type = CMD_UPDATE;
        strncpy(cmd->name, tokens[1], NAME_SIZE - 1);
        cmd->name[NAME_SIZE - 1] = '\0';
        cmd->salary = (uint32_t)strtoul(tokens[2], NULL, 10);
        if (count >= 4) {
            cmd->priority = atoi(tokens[count - 1]);
            cmd->has_priority = 1;
        } else {
            cmd->priority = (*default_priority)++;
            cmd->has_priority = 0;
        }
    } else if (strcmp(tokens[0], "search") == 0) {
        if (count < 2) return -1;
        cmd->type = CMD_SEARCH;
        strncpy(cmd->name, tokens[1], NAME_SIZE - 1);
        cmd->name[NAME_SIZE - 1] = '\0';
        if (count >= 3) {
            cmd->priority = atoi(tokens[count - 1]);
            cmd->has_priority = 1;
        } else {
            cmd->priority = (*default_priority)++;
            cmd->has_priority = 0;
        }
    } else if (strcmp(tokens[0], "print") == 0) {
        cmd->type = CMD_PRINT;
        if (count >= 2) {
            cmd->priority = atoi(tokens[count - 1]);
            cmd->has_priority = 1;
        } else {
            cmd->priority = (*default_priority)++;
            cmd->has_priority = 0;
        }
        cmd->name[0] = '\0';
        cmd->salary = 0;
    } else {
        return -1;
    }

    return 0;
}

int main(void) {
    FILE *fp = fopen(COMMAND_FILE, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", COMMAND_FILE);
        return 1;
    }

    log_file = fopen(LOG_FILE, "w");
    if (!log_file) {
        fprintf(stderr, "Failed to open %s\n", LOG_FILE);
        fclose(fp);
        return 1;
    }

    ht_init();

    pthread_t threads[MAX_COMMANDS];
    int thread_count = 0;
    int default_priority = 0;
    bool turn_initialized = false;
    char line[256];

    while (fgets(line, sizeof(line), fp) && thread_count < MAX_COMMANDS) {
        if (line[0] == '\0' || line[0] == '\n') continue;
        command *cmd = malloc(sizeof(command));
        if (!cmd) break;
        int parse_result = parse_command_line(line, cmd, &default_priority);
        if (parse_result != 0) {
            free(cmd);
            if (parse_result > 0) {
                turn_initialized = true;
                continue; // header line such as "threads" processed
            }
            continue;
        }
        if (!turn_initialized) {
            current_turn = cmd->priority;
            if (cmd->has_priority) {
                default_priority = cmd->priority + 1;
            }
            turn_initialized = true;
        }
        if (!cmd->has_priority) {
            cmd->priority = default_priority - 1;
        }
        pthread_create(&threads[thread_count++], NULL, execute_command, cmd);
    }

    fclose(fp);

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    command final_print = {.type = CMD_PRINT, .priority = current_turn};
    pthread_t final_thread;
    command *final_cmd = malloc(sizeof(command));
    if (final_cmd) {
        *final_cmd = final_print;
        pthread_create(&final_thread, NULL, execute_command, final_cmd);
        pthread_join(final_thread, NULL);
    }

    if (log_file) {
        fclose(log_file);
    }

    return 0;
}

