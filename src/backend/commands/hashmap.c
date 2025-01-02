#include "commands/hashmap.h"
#include "common/hashfn.h"
#include "nodes/parsenodes.h"
#include "utils/hsearch.h"

#define INIT_HASH_SIZE 16
HTAB *
list_to_hash(List *list)
{
	HASHCTL		hash_ctl;
	HTAB	   *hash_table;
	ListCell   *lc;

	memset(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = MAX_OPTION_NAME_SIZE;
	hash_ctl.entrysize = sizeof(HashEntry);

	hash_table = hash_create("model_options", INIT_HASH_SIZE, &hash_ctl, HASH_ELEM);

	foreach(lc, list)
	{
		DefElem    *def = (DefElem *) lfirst(lc);
		Node	   *arg = def->arg;
		HashEntry  *entry;
		char		key[MAX_OPTION_NAME_SIZE];
		bool		found;

		Assert(def->defname != NULL);
		Assert(arg != NULL);

		/* snprintf(key, MAX_OPTION_NAME_SIZE - 1, "%s", def->defname); */
		strncpy(key, def->defname, MAX_OPTION_NAME_SIZE - 1);
		key[MAX_OPTION_NAME_SIZE - 1] = '\0';

		entry = (HashEntry *) hash_search(hash_table, key, HASH_ENTER, &found);
		if (found)
			elog(ERROR, "Duplicate option: %s", def->defname);

		entry->value = arg;
	}

	return hash_table;
}

Node *
get_option(HTAB *hash_table, const char *key)
{
	HashEntry  *entry;
	bool		found;

	Assert(hash_table != NULL);
	Assert(key != NULL);

	entry = (HashEntry *) hash_search(hash_table, key, HASH_FIND, &found);
	if (!found)
		return NULL;

	return entry->value;
}
