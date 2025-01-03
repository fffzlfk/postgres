#include "commands/hashmap.h"
#include "nodes/pg_list.h"
#include "postgres.h"
#include "access/attnum.h"
#include "executor/tuptable.h"
#include "miscadmin.h"
#include "nodes/parsenodes.h"
#include "commands/createmodel.h"
#include "executor/execdesc.h"
#include "executor/executor.h"
#include "rewrite/rewriteHandler.h"
#include "tcop/cmdtag.h"
#include "tcop/dest.h"
#include "tcop/tcopprot.h"
#include "utils/elog.h"
#include "utils/memutils.h"
#include "utils/palloc.h"
#include "utils/portal.h"
#include "utils/snapmgr.h"
#include "utils/snapshot.h"
#include "utils/tuplestore.h"
#include "executor/tstoreReceiver.h"
#include "catalog/pg_model.h"
#include "commands/train.h"
#include "commands/ptensor.h"
#include "commands/modelutils.h"
#include "tcop/pquery.h"

typedef struct
{
	AttrNumber *input_cols;
	AttrNumber *label_cols;
	int			epochs;
	ModelHandle handle;
	HyperParams *params;
}			CreateModelState;

static AttrNumber
get_attribute_id_by_name(TupleDesc tupleDesc, const char *colname)
{
	for (int i = 0; i < tupleDesc->natts; i++)
	{
		Form_pg_attribute attr = TupleDescAttr(tupleDesc, i);

		/* 跳过已经被删除的列（系统可能会有 dropped 列） */
		if (attr->attisdropped)
			continue;

		/* 比较列名是否匹配 */
		if (strcmp(NameStr(attr->attname), colname) == 0)
		{
			/* 返回列的 Attribute Number（从 1 开始计数） */
			return attr->attnum;
		}
	}

	/* 如果没有找到匹配的列，返回 InvalidAttrNumber (-1) */
	return InvalidAttrNumber;
}

static ModelType
parse_model_type(const char *modeltype)
{
	Assert(modeltype != NULL);
	if (strcmp(modeltype, "linear_regression") == 0)
		return LINEAR_REG_MODEL_TYPE;
	else if (strcmp(modeltype, "lstm") == 0)
		return LSTM_MODEL_TYPE;
	else
		elog(ERROR, "Unknown model type: %s", modeltype);

	return UNKNOWN_MODEL_TYPE;
}

static void
init_data_cols(CreateModelStmt * stmt, CreateModelState * state, TupleDesc tupdesc)
{
	ListCell   *lc;
	int			input_size = 0;
	int			label_size = 0;

	Assert(stmt->inputcolumns != NULL);
	Assert(stmt->outputcolumns != NULL);

	state->input_cols = (AttrNumber *) palloc0(sizeof(AttrNumber) * stmt->inputcolumns->length);
	state->label_cols = (AttrNumber *) palloc0(sizeof(AttrNumber) * stmt->labelcolumns->length);

	foreach(lc, stmt->inputcolumns)
	{
		ResTarget  *target = (ResTarget *) lfirst(lc);
		ColumnRef  *colref = (ColumnRef *) target->val;
		const char *colname = strVal(llast(colref->fields));

		AttrNumber	input_col = get_attribute_id_by_name(tupdesc, colname);

		if (input_col == InvalidAttrNumber)
			elog(ERROR, "Input column %s not found in table", colname);

		state->input_cols[input_size++] = input_col;
	}

	foreach(lc, stmt->labelcolumns)
	{
		ResTarget  *target = (ResTarget *) lfirst(lc);
		ColumnRef  *colref = (ColumnRef *) target->val;
		const char *colname = strVal(llast(colref->fields));

		AttrNumber	label_col = get_attribute_id_by_name(tupdesc, colname);

		if (label_col == InvalidAttrNumber)
			elog(ERROR, "Label column %s not found in table", colname);

		state->label_cols[label_size++] = label_col;
	}

	state->params->input_size = input_size;
	state->params->label_size = label_size;
}

static void
parse_model_options(List *modeloptions, ModelType modeltype, CreateModelState * state)
{
	switch (modeltype)
	{
		case LINEAR_REG_MODEL_TYPE:
			break;
		case LSTM_MODEL_TYPE:
			break;
		default:
			elog(ERROR, "Unknown model type: %d", modeltype);
	}
}

#define MAX_EPOCHS 1000
#define DEFAULT_EPOCHS 10

static void
parse_hyper_params(HTAB *options_ht, CreateModelState * state)
{
	{
		Node	   *arg = get_option(options_ht, "lr");
		float		lr;

		if (arg == NULL)
			elog(ERROR, "lr is required");

		if (arg->type != T_Float)
			elog(ERROR, "lr must be a float");

		lr = floatVal(arg);
		if (!(lr > 0.0 && lr < 1.0))
			elog(ERROR, "lr must be in the range (0, 1)");

		state->params->lr = lr;
	}

	{
		Node	   *arg = get_option(options_ht, "epochs");
		int			epochs;

		if (arg == NULL)
		{
			epochs = DEFAULT_EPOCHS;
			state->epochs = epochs;

			elog(INFO, "epochs not specified, using default value: %d", epochs);
		}
		else
		{
			if (arg->type != T_Integer)
				elog(ERROR, "epochs must be an integer");

			epochs = intVal(arg);
			if (epochs <= 0 || epochs > MAX_EPOCHS)
				elog(ERROR, "epochs must be in the range (1, %d)", MAX_EPOCHS);
		}

		state->epochs = epochs;
	}

	{
		Node	   *arg = get_option(options_ht, "batch_size");

		if (arg == NULL)
			elog(ERROR, "batch_size is required");

		if (arg->type != T_Integer)
			elog(ERROR, "batch_size must be an integer");

		state->params->batch_size = intVal(arg);
	}

	{
		Node	   *arg = get_option(options_ht, "opt");

		if (arg == NULL)
		{
			state->params->opt_type = SGD_TYPE;
			elog(INFO, "opt not specified, using default value: %s", "sgd");
		}
		else
		{
			if (arg->type != T_String)
				elog(ERROR, "opt must be a string");

			if (strcmp("sgd", strVal(arg)) == 0)
				state->params->opt_type = SGD_TYPE;
			else if (strcmp("adam", strVal(arg)) == 0)
				state->params->opt_type = ADAM_TYPE;
			else
				elog(ERROR, "opt must be 'sgd' or 'adam'");
		}
	}
}

static CreateModelState *
InitCreateModel(CreateModelStmt * stmt, TupleDesc tupdesc)
{
	ModelType	modelType;
	CreateModelState *state;
	HTAB	   *options_ht;

	modelType = parse_model_type(stmt->modeltype);

	state = (CreateModelState *) palloc0(sizeof(CreateModelState));
	state->params = (HyperParams *) palloc0(sizeof(HyperParams));

	init_data_cols(stmt, state, tupdesc);

	options_ht = list_to_hash(stmt->modeloptions);

	parse_hyper_params(options_ht, state);

	hash_destroy(options_ht);

	switch (modelType)
	{
		case LINEAR_REG_MODEL_TYPE:
			{
				LinearRegressionParams *lg_params =
					(LinearRegressionParams *) palloc0(sizeof(LinearRegressionParams));

				state->handle = create_linear_reg_model(state->params, lg_params);

				pfree(lg_params);
				break;
			}
		case LSTM_MODEL_TYPE:
			{
				break;
			}
		case UNKNOWN_MODEL_TYPE:
			{
				break;
			}
	}

	return state;
}

static void
EndCreateModel(CreateModelState * state)
{
	pfree(state->input_cols);
	pfree(state->label_cols);
	pfree(state->params);
	pfree(state);
}

void
ExecCreateModel(ParseState *pstate, CreateModelStmt * stmt, ParamListInfo params,
				QueryEnvironment *queryEnv, QueryCompletion *qc)
{
	Query	   *query = castNode(Query, stmt->selectquery);
	DestReceiver *dest;
	PlannedStmt *plan;
	Tuplestorestate *tup_store_state;
	TupleTableSlot *slot;
	CreateModelState *cmstate;
	Portal		portal;

	plan = plan_from_query(query, pstate, params);
	portal = CreateNewPortal();
	portal->visible = false;

	/*
	 * Use a snapshot with an updated command ID to ensure this query sees
	 * results of any previously executed queries.  (This could only matter if
	 * the planner executed an allegedly-stable function that changed the
	 * database contents, but let's do it anyway to be parallel to the EXPLAIN
	 * code path.)
	 */
	PushCopiedSnapshot(GetActiveSnapshot());
	UpdateActiveSnapshotCommandId();

	PortalDefineQuery(portal, NULL, pstate->p_sourcetext, CMDTAG_SELECT, list_make1(plan), NULL);

	PortalStart(portal, params, 0, InvalidSnapshot);

	dest = CreateDestReceiver(DestTuplestore);

	tup_store_state = tuplestore_begin_heap(false, true, work_mem);
	tuplestore_set_eflags(tup_store_state, EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD);
	SetTuplestoreDestReceiverParams(dest,
									tup_store_state,
									portal->portalContext,
									false,
									portal->queryDesc->tupDesc,
									NULL);

	cmstate = InitCreateModel(stmt, portal->queryDesc->tupDesc);

	PortalRun(portal, FETCH_ALL, true, dest, NULL, qc);

	slot = MakeSingleTupleTableSlot(portal->queryDesc->tupDesc, &TTSOpsMinimalTuple);

	int			tuple_count = tuplestore_tuple_count(tup_store_state);
	int			batch_size = cmstate->params->batch_size;
	int			input_size = cmstate->params->input_size;
	int			output_size = cmstate->params->label_size;
	int			num_batch = tuple_count / batch_size;

	elog(INFO, "count: %d", tuple_count);

	int			input_dims[2] = {batch_size, input_size};
	int			label_dims[2] = {batch_size, output_size};

	MemoryContext data_cxt =
		AllocSetContextCreate(CurrentMemoryContext, "data context", ALLOCSET_DEFAULT_SIZES);

	MemoryContext old_cxt = MemoryContextSwitchTo(data_cxt);

	PTensor		inputs = AllocPTensor(input_dims, 2);
	PTensor		labels = AllocPTensor(label_dims, 2);

	MemoryContextSwitchTo(old_cxt);

	for (int i = 0; i < cmstate->epochs; i++)
	{
		tuplestore_rescan(tup_store_state);

		ExecClearTuple(slot);

		for (int batch_idx = 0; batch_idx < num_batch; batch_idx++)
		{
			for (int j = 0; j < batch_size; j++)
			{
				bool		has;
				Datum		datum;
				bool		isnull;

				has = tuplestore_gettupleslot(tup_store_state, true, false, slot);
				if (!has)
					break;

				for (int k = 0; k < input_size; k++)
				{
					datum = slot_getattr(slot, cmstate->input_cols[k], &isnull);
					if (isnull)
						elog(ERROR, "null input");
					inputs.data[j * input_size + k] = DatumGetFloat4(datum);
				}

				for (int k = 0; k < output_size; k++)
				{
					datum = slot_getattr(slot, cmstate->label_cols[k], &isnull);
					if (isnull)
						elog(ERROR, "null output");
					labels.data[j * output_size + k] = DatumGetFloat4(datum);
				}

				elog(INFO, "%f, %f", inputs.data[j], labels.data[j]);
			}
			ExecClearTuple(slot);

			float		loss = train_model(cmstate->handle, inputs.data, labels.data, batch_size);

			elog(INFO, "epoch %d, loss: %f", i, loss);
		}
	}

	MemoryContextDelete(data_cxt);

	{
		char	   *model_bin_buf;
		int			model_bin_size;
		bytea	   *model_bin;

		save_model_bin(cmstate->handle, &model_bin_buf, &model_bin_size);
		elog(INFO, "model bin size: %d", model_bin_size);

		model_bin = (bytea *) palloc(VARHDRSZ + model_bin_size);

		SET_VARSIZE(model_bin, VARHDRSZ + model_bin_size);
		memcpy(VARDATA(model_bin), model_bin_buf, model_bin_size);

		SaveModelToCatalog(stmt->modelname, stmt->modeltype, model_bin);

		free(model_bin_buf);
		pfree(model_bin);
	}

	EndCreateModel(cmstate);

	ExecDropSingleTupleTableSlot(slot);

	tuplestore_end(tup_store_state);

	dest->rDestroy(dest);

	PortalDrop(portal, false);

	PopActiveSnapshot();
}
