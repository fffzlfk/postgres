#include "postgres.h"
#include "access/tupdesc.h"
#include "catalog/pg_model_d.h"
#include "postgres.h"
#include "access/htup.h"
#include "catalog/pg_model.h"
#include "utils/relcache.h"
#include "utils/rel.h"
#include "access/table.h"
#include "access/htup_details.h"
#include "catalog/indexing.h"
#include "utils/builtins.h"

void
SaveModelToCatalog(const char *modelname, const char *modeltype, bytea *modelbin)
{
	Relation	modeldesc;
	HeapTuple	tup;
	bool		nulls[Natts_pg_model];
	Datum		values[Natts_pg_model];
	TupleDesc	tupDesc;

	/* elog(INFO, "is not? %d", IsSystemRelation(ModelRelationId)); */

	/* sanity checks */
	if (!modelname)
		elog(ERROR, "no model name supplied");

	if (!modeltype)
		elog(ERROR, "no modeltype supplied");

	modeldesc = table_open(ModelRelationId, RowExclusiveLock);
	tupDesc = modeldesc->rd_att;

	/* initialize nulls and values */
	for (int i = 0; i < Natts_pg_model; i++)
	{
		nulls[i] = false;
		values[i] = (Datum) NULL;
	}

	values[Anum_pg_model_modelname - 1] = CStringGetDatum(modelname);
	values[Anum_pg_model_modeltype - 1] = PointerGetDatum(cstring_to_text(modeltype));
	values[Anum_pg_model_modelbin - 1] = PointerGetDatum(modelbin);

	tup = heap_form_tuple(tupDesc, values, nulls);

	CatalogTupleInsert(modeldesc, tup);

	heap_freetuple(tup);
	table_close(modeldesc, RowExclusiveLock);
}
