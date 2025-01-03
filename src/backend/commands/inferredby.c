#include "commands/inferredby.h"
#include "executor/executor.h"
#include "executor/tstoreReceiver.h"
#include "executor/tuptable.h"
#include "miscadmin.h"
#include "postgres.h"
#include "tcop/dest.h"
#include "tcop/pquery.h"
#include "utils/builtins.h"
#include "commands/modelutils.h"
#include "utils/palloc.h"
#include "utils/portal.h"
#include "utils/snapmgr.h"
#include "utils/tuplestore.h"

void
ExecInferredBy(ParseState *pstate, InferredByStmt * stmt, ParamListInfo params,
			   QueryEnvironment *queryEnv, QueryCompletion *qc, DestReceiver *dest)
{
	Query	   *query = castNode(Query, stmt->selectquery);
	PlannedStmt *plan;
	DestReceiver *from_dest;
	Portal		portal;
	Tuplestorestate *tup_store_state;
	TupOutputState *tup_out_state;

	plan = plan_from_query(query, pstate, params);

	portal = CreateNewPortal();
	portal->visible = false;

	PushCopiedSnapshot(GetActiveSnapshot());
	UpdateActiveSnapshotCommandId();

	PortalDefineQuery(portal, NULL, pstate->p_sourcetext, CMDTAG_SELECT, list_make1(plan), NULL);
	PortalStart(portal, params, 0, InvalidSnapshot);

	from_dest = CreateDestReceiver(DestTuplestore);

	tup_store_state = tuplestore_begin_heap(false, true, work_mem);
	tuplestore_set_eflags(tup_store_state, EXEC_FLAG_BACKWARD);
	SetTuplestoreDestReceiverParams(from_dest, tup_store_state, portal->portalContext, false, portal->queryDesc->tupDesc, NULL);

	PortalRun(portal, FETCH_ALL, true, from_dest, NULL, qc);

	tup_out_state = begin_tup_output_tupdesc(dest, portal->queryDesc->tupDesc, &TTSOpsVirtual);

	elog(INFO, "count: %d", tuplestore_tuple_count(tup_store_state));

	int			n = portal->queryDesc->tupDesc->natts;

	for (int k = 0; k < 3; k++)
	{
		Datum	   *values = palloc(n * sizeof(Datum));
		bool	   *nulls = palloc(n * sizeof(bool));

		for (int i = 0; i < n; i++)
		{
			values[i] = Float4GetDatum(1.0);
			nulls[i] = 0;
		}

		do_tup_output(tup_out_state, values, nulls);
	}

	end_tup_output(tup_out_state);

	tuplestore_end(tup_store_state);

	from_dest->rDestroy(from_dest);

	if (qc)
		SetQueryCompletion(qc, CMDTAG_SELECT, portal->queryDesc->estate->es_processed);

	PortalDrop(portal, false);

	PopActiveSnapshot();
}
