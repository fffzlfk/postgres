#ifndef INFERREDBY_H
#define INFERREDBY_H

#include "postgres.h"
#include "nodes/params.h"
#include "parser/parse_node.h"
#include "nodes/parsenodes.h"
#include "tcop/cmdtag.h"
#include "tcop/dest.h"

extern void ExecInferredBy(ParseState *pstate, InferredByStmt * stmt, ParamListInfo params, QueryEnvironment *queryEnv, QueryCompletion *qc, DestReceiver *dest);
