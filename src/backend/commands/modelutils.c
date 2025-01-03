#include "commands/modelutils.h"
#include "rewrite/rewriteHandler.h"
#include "tcop/tcopprot.h"

PlannedStmt *
plan_from_query(Query *query, ParseState *pstate, ParamListInfo params)
{
	List	   *rewritten;

	Assert(query->commandType == CMD_SELECT);

	rewritten = QueryRewrite(query);

	if (list_length(rewritten) != 1)
		elog(ERROR, "unexpected rewrite result");

	query = linitial_node(Query, rewritten);

	Assert(query->type == CMD_SELECT);

	return pg_plan_query(query, pstate->p_sourcetext, CURSOR_OPT_PARALLEL_OK, params);
}
