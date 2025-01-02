#ifndef CREATEMODEL_H
#define CREATEMODEL_H

#include "nodes/params.h"
#include "nodes/parsenodes.h"
#include "parser/parse_node.h"
#include "tcop/cmdtag.h"

extern void ExecCreateModel(ParseState *pstate, CreateModelStmt * stmt,
							ParamListInfo params, QueryEnvironment *queryEnv,
							QueryCompletion *qc);
