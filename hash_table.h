#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define NAME_SIZE 50

typedef struct hash_struct {
    uint32_t hash;
    char name[NAME_SIZE];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

void ht_init(void);

bool ht_insert(uint32_t hash, const char *name, uint32_t salary);

bool ht_delete(uint32_t hash, char *removed_name, uint32_t *removed_salary);

bool ht_update(uint32_t hash, uint32_t new_salary, uint32_t *old_salary, char *name_out);

bool ht_search(uint32_t hash, char *name_out, uint32_t *salary_out);

void ht_print(FILE *out);

#endif
