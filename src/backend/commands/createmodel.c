#include "postgres.h"
#include "executor/tuptable.h"
#include "miscadmin.h"
#include "nodes/parsenodes.h"
#include "commands/createmodel.h"
#include "executor/execdesc.h"
#include "executor/executor.h"
#include "rewrite/rewriteHandler.h"
#include "tcop/dest.h"
#include "tcop/tcopprot.h"
#include "utils/elog.h"
#include "utils/memutils.h"
#include "utils/palloc.h"
#include "utils/snapmgr.h"
#include "utils/snapshot.h"
#include "utils/tuplestore.h"
#include "executor/tstoreReceiver.h"

typedef struct
{
	DestReceiver pub; /* publicly-known function pointers */
	int receiveCount;
} DR_model;

static bool
model_receive(TupleTableSlot *slot, DestReceiver *self)
{
	DR_model *modelState = (DR_model *) self;

	// print all the tuples that have been passed to us
	bool isnull;
	Datum datum;
	datum = slot_getattr(slot, 1, &isnull);
	if (!isnull)
		elog(INFO, "Received tuple: %d", DatumGetInt32(datum));

	modelState->receiveCount++;

	if (modelState->receiveCount == 10)
		elog(INFO, "Received 10 tuples");

	return true;
}
static void
model_startup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
}

static void
model_shutdown(DestReceiver *self)
{
}

static void
model_destroy(DestReceiver *self)
{
	pfree(self);
}
DestReceiver *
CreateModelReceiver()
{
	DR_model *self = (DR_model *) palloc0(sizeof(DR_model));

	self->pub.receiveSlot = model_receive;
	self->pub.rStartup = model_startup;
	self->pub.rShutdown = model_shutdown;
	self->pub.rDestroy = model_destroy;
	self->pub.mydest = DestTuplestore;
	/* other private fields will be set during intorel_startup */

	return (DestReceiver *) self;
}

void
ExecCreateModel(ParseState *pstate, CreateModelStmt *stmt, ParamListInfo params,
				QueryEnvironment *queryEnv, QueryCompletion *qc)
{
	Query *query = castNode(Query, stmt->selectquery);
	List *rewritten;
	DestReceiver *dest;
	QueryDesc *queryDesc;
	PlannedStmt *plan;

	Assert(query->commandType == CMD_SELECT);

	dest = CreateModelReceiver();
	rewritten = QueryRewrite(query);

	if (list_length(rewritten) != 1)
		elog(ERROR, "unexpected rewrite result for CREATE MODEL FROM SELECT");

	query = linitial_node(Query, rewritten);

	Assert(query->commandType == CMD_SELECT);

	plan = pg_plan_query(query, pstate->p_sourcetext, CURSOR_OPT_PARALLEL_OK, params);

	/*
	 * Use a snapshot with an updated command ID to ensure this query sees
	 * results of any previously executed queries.  (This could only
	 * matter if the planner executed an allegedly-stable function that
	 * changed the database contents, but let's do it anyway to be
	 * parallel to the EXPLAIN code path.)
	 */
	PushCopiedSnapshot(GetActiveSnapshot());
	UpdateActiveSnapshotCommandId();

	/* Create a QueryDesc, redirecting output to our tuple receiver */
	queryDesc = CreateQueryDesc(plan,
								pstate->p_sourcetext,
								GetActiveSnapshot(),
								InvalidSnapshot,
								dest,
								params,
								queryEnv,
								0);

	/* call ExecutorStart to prepare the plan for execution */
	ExecutorStart(queryDesc, EXEC_FLAG_BACKWARD);

	/* run the plan to completion */
	ExecutorRun(queryDesc, ForwardScanDirection, 0);

	/* save the rowcount if we're given a qc to fill */
	if (qc)
		SetQueryCompletion(qc, CMDTAG_SELECT, queryDesc->estate->es_processed);

	/* and clean up */
	ExecutorFinish(queryDesc);
	ExecutorEnd(queryDesc);

	FreeQueryDesc(queryDesc);

	PopActiveSnapshot();
}
