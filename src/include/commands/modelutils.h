#ifndef MODELUTIL_H
#define MODELUTIL_H

#include "postgres.h"
#include "nodes/plannodes.h"
#include "nodes/parsenodes.h"
#include "parser/parse_node.h"
#include "nodes/params.h"

extern PlannedStmt *plan_from_query(Query *query, ParseState *pstate, ParamListInfo params);
