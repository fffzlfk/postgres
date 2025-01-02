
#ifndef PG_MODEL_H
#define PG_MODEL_H

#include "catalog/genbki.h"
#include "catalog/pg_model_d.h"

CATALOG(pg_model, 9999, ModelRelationId)
{
	/* Language name */
	NameData modelname;

#ifdef CATALOG_VARLEN /* variable-length fields start here */

	text modeltype;

	bytea modelbin BKI_DEFAULT(_null_);
	/* Access privileges */
#endif
}
FormData_pg_model;

typedef FormData_pg_model *Form_pg_model;

extern void SaveModelToCatalog(const char *modelname, const char *modeltype, bytea *modelbin);

#endif
