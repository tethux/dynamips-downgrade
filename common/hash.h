#pragma once

#include "dynamips_common.h"

typedef struct hash_table hash_table_t;
typedef unsigned (*hash_fcompute)(void *);
typedef int (*hash_fcompare)(void *, void *);
typedef void (*hash_fforeach)(void *, void *, void *);

#ifdef __cplusplus
extern "C" {
#endif

unsigned str_hash(void *);
hash_table_t *hash_table_create(hash_fcompute, hash_fcompare, int);
void hash_table_delete(hash_table_t *);
int hash_table_foreach(hash_table_t *, hash_fforeach, void *);
int hash_table_insert(hash_table_t *, void *, void *);
void *hash_table_lookup(hash_table_t *, void *);
void *hash_table_lookup_dcmp(hash_table_t *, void *);
void *hash_table_remove(hash_table_t *, void *);

#ifdef __cplusplus
}
#endif
