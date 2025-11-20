#include "hash_table.h"
#include <stdlib.h>
#include <string.h>

static hashRecord *head = NULL;

void ht_init(void) {
    head = NULL;
}

bool ht_insert(uint32_t hash, const char *name, uint32_t salary) {
    hashRecord *prev = NULL;
    hashRecord *curr = head;
    while (curr && curr->hash < hash) {
        prev = curr;
        curr = curr->next;
    }
    if (curr && curr->hash == hash) {
        return false;
    }

    hashRecord *node = malloc(sizeof(hashRecord));
    if (!node) {
        return false;
    }
    node->hash = hash;
    strncpy(node->name, name, NAME_SIZE - 1);
    node->name[NAME_SIZE - 1] = '\0';
    node->salary = salary;
    node->next = curr;

    if (prev) {
        prev->next = node;
    } else {
        head = node;
    }
    return true;
}

bool ht_delete(uint32_t hash, char *removed_name, uint32_t *removed_salary) {
    hashRecord *prev = NULL;
    hashRecord *curr = head;
    while (curr && curr->hash < hash) {
        prev = curr;
        curr = curr->next;
    }
    if (!curr || curr->hash != hash) {
        return false;
    }

    if (removed_name) {
        strncpy(removed_name, curr->name, NAME_SIZE);
        removed_name[NAME_SIZE - 1] = '\0';
    }
    if (removed_salary) {
        *removed_salary = curr->salary;
    }

    if (prev) {
        prev->next = curr->next;
    } else {
        head = curr->next;
    }
    free(curr);
    return true;
}

bool ht_update(uint32_t hash, uint32_t new_salary, uint32_t *old_salary, char *name_out) {
    hashRecord *curr = head;
    while (curr && curr->hash < hash) {
        curr = curr->next;
    }
    if (!curr || curr->hash != hash) {
        return false;
    }
    if (old_salary) {
        *old_salary = curr->salary;
    }
    if (name_out) {
        strncpy(name_out, curr->name, NAME_SIZE);
        name_out[NAME_SIZE - 1] = '\0';
    }
    curr->salary = new_salary;
    return true;
}

bool ht_search(uint32_t hash, char *name_out, uint32_t *salary_out) {
    hashRecord *curr = head;
    while (curr && curr->hash < hash) {
        curr = curr->next;
    }
    if (!curr || curr->hash != hash) {
        return false;
    }
    if (name_out) {
        strncpy(name_out, curr->name, NAME_SIZE);
        name_out[NAME_SIZE - 1] = '\0';
    }
    if (salary_out) {
        *salary_out = curr->salary;
    }
    return true;
}

void ht_print(FILE *out) {
    hashRecord *curr = head;
    while (curr) {
        fprintf(out, "%u,%s,%u\n", curr->hash, curr->name, curr->salary);
        curr = curr->next;
    }
}

