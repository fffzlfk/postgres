
#ifndef PG_MODEL_H
#define PG_MODEL_H

#include "catalog/genbki.h"
#include "catalog/pg_model_d.h"

CATALOG(pg_model, 9999, ModelRelationId)
{
	/* Language name */
	NameData	modelname;

	text		modeltype;

	bytea		modelbin BKI_DEFAULT(_null_);

	/* Access privileges */
}

FormData_pg_model;

typedef FormData_pg_model * Form_pg_model;

DECLARE_UNIQUE_INDEX_PKEY(pg_modelname_index, 9998, ModelNameIndexId, pg_model, btree(modelname name_ops));

extern void SaveModelToCatalog(const char *modelname, const char *modeltype, bytea *modelbin);

#endif
