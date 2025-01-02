#ifndef HASHMAP_H
#define HASHMAP_H

#include "postgres.h"
#include "nodes/pg_list.h"
#include "utils/hsearch.h"

#define MAX_OPTION_NAME_SIZE 32

typedef struct
{
	char		key[MAX_OPTION_NAME_SIZE];	/* 键 */
	Node	   *value;			/* 值 */
}			HashEntry;

extern HTAB *list_to_hash(List *list);

extern Node *get_option(HTAB *hash_table, const char *key);
