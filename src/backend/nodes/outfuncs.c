/*-------------------------------------------------------------------------
 *
 * outfuncs.c
 *	  Output functions for Postgres tree nodes.
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header: /cvsroot/pgsql/src/backend/nodes/outfuncs.c,v 1.182 2002/11/25 18:12:09 tgl Exp $
 *
 * NOTES
 *	  Every node type that can appear in stored rules' parsetrees *must*
 *	  have an output function defined here (as well as an input function
 *	  in readfuncs.c).  For use in debugging, we also provide output
 *	  functions for nodes that appear in raw parsetrees and plan trees.
 *	  These nodes however need not have input functions.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "lib/stringinfo.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "nodes/primnodes.h"
#include "nodes/relation.h"
#include "parser/parse.h"
#include "utils/datum.h"


/*
 * Macros to simplify output of different kinds of fields.  Use these
 * wherever possible to reduce the chance for silly typos.  Note that these
 * hard-wire conventions about the names of the local variables in an Out
 * routine.
 */

/* Write the label for the node type */
#define WRITE_NODE_TYPE(nodelabel) \
	appendStringInfo(str, nodelabel)

/* Write an integer field (anything written as ":fldname %d") */
#define WRITE_INT_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %d", node->fldname)

/* Write an unsigned integer field (anything written as ":fldname %u") */
#define WRITE_UINT_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %u", node->fldname)

/* Write an OID field (don't hard-wire assumption that OID is same as uint) */
#define WRITE_OID_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %u", node->fldname)

/* Write a long-integer field */
#define WRITE_LONG_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %ld", node->fldname)

/* Write a char field (ie, one ascii character) */
#define WRITE_CHAR_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %c", node->fldname)

/* Write an enumerated-type field as an integer code */
#define WRITE_ENUM_FIELD(fldname, enumtype) \
	appendStringInfo(str, " :" CppAsString(fldname) " %d", \
					 (int) node->fldname)

/* Write a float field --- caller must give format to define precision */
#define WRITE_FLOAT_FIELD(fldname,format) \
	appendStringInfo(str, " :" CppAsString(fldname) " " format, node->fldname)

/* Write a boolean field */
#define WRITE_BOOL_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %s", \
					 booltostr(node->fldname))

/* Write a character-string (possibly NULL) field */
#define WRITE_STRING_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outToken(str, node->fldname))

/* Write a Node field */
#define WRITE_NODE_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outNode(str, node->fldname))

/* Write an integer-list field */
#define WRITE_INTLIST_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outIntList(str, node->fldname))

/* Write an OID-list field */
#define WRITE_OIDLIST_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outOidList(str, node->fldname))


#define booltostr(x)  ((x) ? "true" : "false")

static void _outDatum(StringInfo str, Datum value, int typlen, bool typbyval);
static void _outNode(StringInfo str, void *obj);

/*
 * _outToken
 *	  Convert an ordinary string (eg, an identifier) into a form that
 *	  will be decoded back to a plain token by read.c's functions.
 *
 *	  If a null or empty string is given, it is encoded as "<>".
 */
static void
_outToken(StringInfo str, char *s)
{
	if (s == NULL || *s == '\0')
	{
		appendStringInfo(str, "<>");
		return;
	}

	/*
	 * Look for characters or patterns that are treated specially by
	 * read.c (either in pg_strtok() or in nodeRead()), and therefore need
	 * a protective backslash.
	 */
	/* These characters only need to be quoted at the start of the string */
	if (*s == '<' ||
		*s == '\"' ||
		*s == '@' ||
		isdigit((unsigned char) *s) ||
		((*s == '+' || *s == '-') &&
		 (isdigit((unsigned char) s[1]) || s[1] == '.')))
		appendStringInfoChar(str, '\\');
	while (*s)
	{
		/* These chars must be backslashed anywhere in the string */
		if (*s == ' ' || *s == '\n' || *s == '\t' ||
			*s == '(' || *s == ')' || *s == '{' || *s == '}' ||
			*s == '\\')
			appendStringInfoChar(str, '\\');
		appendStringInfoChar(str, *s++);
	}
}

/*
 * _outIntList -
 *	   converts a List of integers
 */
static void
_outIntList(StringInfo str, List *list)
{
	List	   *l;

	appendStringInfoChar(str, '(');
	foreach(l, list)
		appendStringInfo(str, " %d", lfirsti(l));
	appendStringInfoChar(str, ')');
}

/*
 * _outOidList -
 *	   converts a List of OIDs
 */
static void
_outOidList(StringInfo str, List *list)
{
	List	   *l;

	appendStringInfoChar(str, '(');
	foreach(l, list)
		appendStringInfo(str, " %u", (Oid) lfirsti(l));
	appendStringInfoChar(str, ')');
}

static void
_outCreateStmt(StringInfo str, CreateStmt *node)
{
	WRITE_NODE_TYPE("CREATE");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(tableElts);
	WRITE_NODE_FIELD(inhRelations);
	WRITE_NODE_FIELD(constraints);
	WRITE_BOOL_FIELD(hasoids);
	WRITE_ENUM_FIELD(oncommit, OnCommitAction);
}

static void
_outIndexStmt(StringInfo str, IndexStmt *node)
{
	WRITE_NODE_TYPE("INDEX");

	WRITE_STRING_FIELD(idxname);
	WRITE_NODE_FIELD(relation);
	WRITE_STRING_FIELD(accessMethod);
	WRITE_NODE_FIELD(indexParams);
	WRITE_NODE_FIELD(whereClause);
	WRITE_NODE_FIELD(rangetable);
	WRITE_BOOL_FIELD(unique);
	WRITE_BOOL_FIELD(primary);
	WRITE_BOOL_FIELD(isconstraint);
}

static void
_outNotifyStmt(StringInfo str, NotifyStmt *node)
{
	WRITE_NODE_TYPE("NOTIFY");

	WRITE_NODE_FIELD(relation);
}

static void
_outSelectStmt(StringInfo str, SelectStmt *node)
{
	WRITE_NODE_TYPE("SELECT");

	/* XXX this is pretty durn incomplete */
	WRITE_NODE_FIELD(whereClause);
}

static void
_outFuncCall(StringInfo str, FuncCall *node)
{
	WRITE_NODE_TYPE("FUNCCALL");

	WRITE_NODE_FIELD(funcname);
	WRITE_NODE_FIELD(args);
	WRITE_BOOL_FIELD(agg_star);
	WRITE_BOOL_FIELD(agg_distinct);
}

static void
_outColumnDef(StringInfo str, ColumnDef *node)
{
	WRITE_NODE_TYPE("COLUMNDEF");

	WRITE_STRING_FIELD(colname);
	WRITE_NODE_FIELD(typename);
	WRITE_INT_FIELD(inhcount);
	WRITE_BOOL_FIELD(is_local);
	WRITE_BOOL_FIELD(is_not_null);
	WRITE_NODE_FIELD(raw_default);
	WRITE_STRING_FIELD(cooked_default);
	WRITE_NODE_FIELD(constraints);
	WRITE_NODE_FIELD(support);
}

static void
_outTypeName(StringInfo str, TypeName *node)
{
	WRITE_NODE_TYPE("TYPENAME");

	WRITE_NODE_FIELD(names);
	WRITE_OID_FIELD(typeid);
	WRITE_BOOL_FIELD(timezone);
	WRITE_BOOL_FIELD(setof);
	WRITE_BOOL_FIELD(pct_type);
	WRITE_INT_FIELD(typmod);
	WRITE_NODE_FIELD(arrayBounds);
}

static void
_outTypeCast(StringInfo str, TypeCast *node)
{
	WRITE_NODE_TYPE("TYPECAST");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(typename);
}

static void
_outIndexElem(StringInfo str, IndexElem *node)
{
	WRITE_NODE_TYPE("INDEXELEM");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(funcname);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(opclass);
}

static void
_outQuery(StringInfo str, Query *node)
{
	WRITE_NODE_TYPE("QUERY");

	WRITE_ENUM_FIELD(commandType, CmdType);
	WRITE_ENUM_FIELD(querySource, QuerySource);

	/*
	 * Hack to work around missing outfuncs routines for a lot of the
	 * utility-statement node types.  (The only one we actually *need* for
	 * rules support is NotifyStmt.)  Someday we ought to support 'em all,
	 * but for the meantime do this to avoid getting lots of warnings when
	 * running with debug_print_parse on.
	 */
	if (node->utilityStmt)
	{
		switch (nodeTag(node->utilityStmt))
		{
			case T_CreateStmt:
			case T_IndexStmt:
			case T_NotifyStmt:
				WRITE_NODE_FIELD(utilityStmt);
				break;
			default:
				appendStringInfo(str, " :utilityStmt ?");
				break;
		}
	}
	else
		appendStringInfo(str, " :utilityStmt <>");

	WRITE_INT_FIELD(resultRelation);
	WRITE_NODE_FIELD(into);
	WRITE_BOOL_FIELD(isPortal);
	WRITE_BOOL_FIELD(isBinary);
	WRITE_BOOL_FIELD(hasAggs);
	WRITE_BOOL_FIELD(hasSubLinks);
	WRITE_NODE_FIELD(rtable);
	WRITE_NODE_FIELD(jointree);
	WRITE_INTLIST_FIELD(rowMarks);
	WRITE_NODE_FIELD(targetList);
	WRITE_NODE_FIELD(groupClause);
	WRITE_NODE_FIELD(havingQual);
	WRITE_NODE_FIELD(distinctClause);
	WRITE_NODE_FIELD(sortClause);
	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
	WRITE_NODE_FIELD(setOperations);
	WRITE_INTLIST_FIELD(resultRelations);

	/* planner-internal fields are not written out */
}

static void
_outSortClause(StringInfo str, SortClause *node)
{
	WRITE_NODE_TYPE("SORTCLAUSE");

	WRITE_UINT_FIELD(tleSortGroupRef);
	WRITE_OID_FIELD(sortop);
}

static void
_outGroupClause(StringInfo str, GroupClause *node)
{
	WRITE_NODE_TYPE("GROUPCLAUSE");

	WRITE_UINT_FIELD(tleSortGroupRef);
	WRITE_OID_FIELD(sortop);
}

static void
_outSetOperationStmt(StringInfo str, SetOperationStmt *node)
{
	WRITE_NODE_TYPE("SETOPERATIONSTMT");

	WRITE_ENUM_FIELD(op, SetOperation);
	WRITE_BOOL_FIELD(all);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
	WRITE_OIDLIST_FIELD(colTypes);
}

/*
 *	Stuff from plannodes.h
 */

/*
 * print the basic stuff of all nodes that inherit from Plan
 *
 * NOTE: we deliberately omit the execution state (EState)
 */
static void
_outPlanInfo(StringInfo str, Plan *node)
{
	WRITE_FLOAT_FIELD(startup_cost, "%.2f");
	WRITE_FLOAT_FIELD(total_cost, "%.2f");
	WRITE_FLOAT_FIELD(plan_rows, "%.0f");
	WRITE_INT_FIELD(plan_width);
	WRITE_NODE_FIELD(targetlist);
	WRITE_NODE_FIELD(qual);
	WRITE_NODE_FIELD(lefttree);
	WRITE_NODE_FIELD(righttree);
	WRITE_INTLIST_FIELD(extParam);
	WRITE_INTLIST_FIELD(locParam);
	/* chgParam is execution state too */
	WRITE_NODE_FIELD(initPlan);
	/* we don't write subPlan; reader must reconstruct list */
	WRITE_INT_FIELD(nParamExec);
}

/*
 * print the basic stuff of all nodes that inherit from Scan
 */
static void
_outScanInfo(StringInfo str, Scan *node)
{
	_outPlanInfo(str, (Plan *) node);

	WRITE_UINT_FIELD(scanrelid);
}

/*
 * print the basic stuff of all nodes that inherit from Join
 */
static void
_outJoinPlanInfo(StringInfo str, Join *node)
{
	_outPlanInfo(str, (Plan *) node);

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_NODE_FIELD(joinqual);
}


static void
_outPlan(StringInfo str, Plan *node)
{
	WRITE_NODE_TYPE("PLAN");

	_outPlanInfo(str, (Plan *) node);
}

static void
_outResult(StringInfo str, Result *node)
{
	WRITE_NODE_TYPE("RESULT");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(resconstantqual);
}

static void
_outAppend(StringInfo str, Append *node)
{
	WRITE_NODE_TYPE("APPEND");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(appendplans);
	WRITE_BOOL_FIELD(isTarget);
}

static void
_outScan(StringInfo str, Scan *node)
{
	WRITE_NODE_TYPE("SCAN");

	_outScanInfo(str, (Scan *) node);
}

static void
_outSeqScan(StringInfo str, SeqScan *node)
{
	WRITE_NODE_TYPE("SEQSCAN");

	_outScanInfo(str, (Scan *) node);
}

static void
_outIndexScan(StringInfo str, IndexScan *node)
{
	WRITE_NODE_TYPE("INDEXSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_OIDLIST_FIELD(indxid);
	WRITE_NODE_FIELD(indxqual);
	WRITE_NODE_FIELD(indxqualorig);
	WRITE_ENUM_FIELD(indxorderdir, ScanDirection);
}

static void
_outTidScan(StringInfo str, TidScan *node)
{
	WRITE_NODE_TYPE("TIDSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_BOOL_FIELD(needRescan);
	WRITE_NODE_FIELD(tideval);
}

static void
_outSubqueryScan(StringInfo str, SubqueryScan *node)
{
	WRITE_NODE_TYPE("SUBQUERYSCAN");

	_outScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(subplan);
}

static void
_outFunctionScan(StringInfo str, FunctionScan *node)
{
	WRITE_NODE_TYPE("FUNCTIONSCAN");

	_outScanInfo(str, (Scan *) node);
}

static void
_outJoin(StringInfo str, Join *node)
{
	WRITE_NODE_TYPE("JOIN");

	_outJoinPlanInfo(str, (Join *) node);
}

static void
_outNestLoop(StringInfo str, NestLoop *node)
{
	WRITE_NODE_TYPE("NESTLOOP");

	_outJoinPlanInfo(str, (Join *) node);
}

static void
_outMergeJoin(StringInfo str, MergeJoin *node)
{
	WRITE_NODE_TYPE("MERGEJOIN");

	_outJoinPlanInfo(str, (Join *) node);

	WRITE_NODE_FIELD(mergeclauses);
}

static void
_outHashJoin(StringInfo str, HashJoin *node)
{
	WRITE_NODE_TYPE("HASHJOIN");

	_outJoinPlanInfo(str, (Join *) node);

	WRITE_NODE_FIELD(hashclauses);
	WRITE_OID_FIELD(hashjoinop);
}

static void
_outAgg(StringInfo str, Agg *node)
{
	WRITE_NODE_TYPE("AGG");

	_outPlanInfo(str, (Plan *) node);

	WRITE_ENUM_FIELD(aggstrategy, AggStrategy);
	WRITE_INT_FIELD(numCols);
	WRITE_LONG_FIELD(numGroups);
}

static void
_outGroup(StringInfo str, Group *node)
{
	WRITE_NODE_TYPE("GRP");

	_outPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(numCols);
}

static void
_outMaterial(StringInfo str, Material *node)
{
	WRITE_NODE_TYPE("MATERIAL");

	_outPlanInfo(str, (Plan *) node);
}

static void
_outSort(StringInfo str, Sort *node)
{
	WRITE_NODE_TYPE("SORT");

	_outPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(keycount);
}

static void
_outUnique(StringInfo str, Unique *node)
{
	int			i;

	WRITE_NODE_TYPE("UNIQUE");

	_outPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(numCols);

	appendStringInfo(str, " :uniqColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->uniqColIdx[i]);
}

static void
_outSetOp(StringInfo str, SetOp *node)
{
	int			i;

	WRITE_NODE_TYPE("SETOP");

	_outPlanInfo(str, (Plan *) node);

	WRITE_ENUM_FIELD(cmd, SetOpCmd);
	WRITE_INT_FIELD(numCols);

	appendStringInfo(str, " :dupColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->dupColIdx[i]);

	WRITE_INT_FIELD(flagColIdx);
}

static void
_outLimit(StringInfo str, Limit *node)
{
	WRITE_NODE_TYPE("LIMIT");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
}

static void
_outHash(StringInfo str, Hash *node)
{
	WRITE_NODE_TYPE("HASH");

	_outPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(hashkey);
}

static void
_outSubPlan(StringInfo str, SubPlan *node)
{
	WRITE_NODE_TYPE("SUBPLAN");

	WRITE_NODE_FIELD(plan);
	WRITE_INT_FIELD(plan_id);
	WRITE_NODE_FIELD(rtable);
	WRITE_INTLIST_FIELD(setParam);
	WRITE_INTLIST_FIELD(parParam);
	WRITE_NODE_FIELD(sublink);
}

/*****************************************************************************
 *
 *	Stuff from primnodes.h.
 *
 *****************************************************************************/

static void
_outResdom(StringInfo str, Resdom *node)
{
	WRITE_NODE_TYPE("RESDOM");

	WRITE_INT_FIELD(resno);
	WRITE_OID_FIELD(restype);
	WRITE_INT_FIELD(restypmod);
	WRITE_STRING_FIELD(resname);
	WRITE_UINT_FIELD(ressortgroupref);
	WRITE_UINT_FIELD(reskey);
	WRITE_OID_FIELD(reskeyop);
	WRITE_BOOL_FIELD(resjunk);
}

static void
_outExpr(StringInfo str, Expr *node)
{
	char	   *opstr = NULL;

	WRITE_NODE_TYPE("EXPR");

	WRITE_OID_FIELD(typeOid);

	/* do-it-yourself enum representation */
	switch (node->opType)
	{
		case OP_EXPR:
			opstr = "op";
			break;
		case DISTINCT_EXPR:
			opstr = "distinct";
			break;
		case FUNC_EXPR:
			opstr = "func";
			break;
		case OR_EXPR:
			opstr = "or";
			break;
		case AND_EXPR:
			opstr = "and";
			break;
		case NOT_EXPR:
			opstr = "not";
			break;
		case SUBPLAN_EXPR:
			opstr = "subp";
			break;
	}
	appendStringInfo(str, " :opType ");
	_outToken(str, opstr);

	WRITE_NODE_FIELD(oper);
	WRITE_NODE_FIELD(args);
}

static void
_outVar(StringInfo str, Var *node)
{
	WRITE_NODE_TYPE("VAR");

	WRITE_UINT_FIELD(varno);
	WRITE_INT_FIELD(varattno);
	WRITE_OID_FIELD(vartype);
	WRITE_INT_FIELD(vartypmod);
	WRITE_UINT_FIELD(varlevelsup);
	WRITE_UINT_FIELD(varnoold);
	WRITE_INT_FIELD(varoattno);
}

static void
_outConst(StringInfo str, Const *node)
{
	WRITE_NODE_TYPE("CONST");

	WRITE_OID_FIELD(consttype);
	WRITE_INT_FIELD(constlen);
	WRITE_BOOL_FIELD(constbyval);
	WRITE_BOOL_FIELD(constisnull);
	/* XXX what about constisset, constiscast? */

	appendStringInfo(str, " :constvalue ");
	if (node->constisnull)
		appendStringInfo(str, "<>");
	else
		_outDatum(str, node->constvalue, node->constlen, node->constbyval);
}

static void
_outAggref(StringInfo str, Aggref *node)
{
	WRITE_NODE_TYPE("AGGREF");

	WRITE_OID_FIELD(aggfnoid);
	WRITE_OID_FIELD(aggtype);
	WRITE_NODE_FIELD(target);
	WRITE_BOOL_FIELD(aggstar);
	WRITE_BOOL_FIELD(aggdistinct);
	/* aggno is not saved since it is just executor state */
}

static void
_outSubLink(StringInfo str, SubLink *node)
{
	WRITE_NODE_TYPE("SUBLINK");

	WRITE_ENUM_FIELD(subLinkType, SubLinkType);
	WRITE_BOOL_FIELD(useor);
	WRITE_NODE_FIELD(lefthand);
	WRITE_NODE_FIELD(oper);
	WRITE_NODE_FIELD(subselect);
}

static void
_outArrayRef(StringInfo str, ArrayRef *node)
{
	WRITE_NODE_TYPE("ARRAYREF");

	WRITE_OID_FIELD(refrestype);
	WRITE_INT_FIELD(refattrlength);
	WRITE_INT_FIELD(refelemlength);
	WRITE_BOOL_FIELD(refelembyval);
	WRITE_CHAR_FIELD(refelemalign);
	WRITE_NODE_FIELD(refupperindexpr);
	WRITE_NODE_FIELD(reflowerindexpr);
	WRITE_NODE_FIELD(refexpr);
	WRITE_NODE_FIELD(refassgnexpr);
}

static void
_outFunc(StringInfo str, Func *node)
{
	WRITE_NODE_TYPE("FUNC");

	WRITE_OID_FIELD(funcid);
	WRITE_OID_FIELD(funcresulttype);
	WRITE_BOOL_FIELD(funcretset);
	WRITE_ENUM_FIELD(funcformat, CoercionForm);
}

static void
_outOper(StringInfo str, Oper *node)
{
	WRITE_NODE_TYPE("OPER");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opid);
	WRITE_OID_FIELD(opresulttype);
	WRITE_BOOL_FIELD(opretset);
}

static void
_outParam(StringInfo str, Param *node)
{
	WRITE_NODE_TYPE("PARAM");

	WRITE_INT_FIELD(paramkind);
	WRITE_INT_FIELD(paramid);
	WRITE_STRING_FIELD(paramname);
	WRITE_OID_FIELD(paramtype);
}

static void
_outFieldSelect(StringInfo str, FieldSelect *node)
{
	WRITE_NODE_TYPE("FIELDSELECT");

	WRITE_NODE_FIELD(arg);
	WRITE_INT_FIELD(fieldnum);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
}

static void
_outRelabelType(StringInfo str, RelabelType *node)
{
	WRITE_NODE_TYPE("RELABELTYPE");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
	WRITE_ENUM_FIELD(relabelformat, CoercionForm);
}

static void
_outRangeTblRef(StringInfo str, RangeTblRef *node)
{
	WRITE_NODE_TYPE("RANGETBLREF");

	WRITE_INT_FIELD(rtindex);
}

static void
_outJoinExpr(StringInfo str, JoinExpr *node)
{
	WRITE_NODE_TYPE("JOINEXPR");

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_BOOL_FIELD(isNatural);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
	WRITE_NODE_FIELD(using);
	WRITE_NODE_FIELD(quals);
	WRITE_NODE_FIELD(alias);
	WRITE_INT_FIELD(rtindex);
}

static void
_outFromExpr(StringInfo str, FromExpr *node)
{
	WRITE_NODE_TYPE("FROMEXPR");

	WRITE_NODE_FIELD(fromlist);
	WRITE_NODE_FIELD(quals);
}

static void
_outTargetEntry(StringInfo str, TargetEntry *node)
{
	WRITE_NODE_TYPE("TARGETENTRY");

	WRITE_NODE_FIELD(resdom);
	/* fjoin not supported ... */
	WRITE_NODE_FIELD(expr);
}

static void
_outAlias(StringInfo str, Alias *node)
{
	WRITE_NODE_TYPE("ALIAS");

	WRITE_STRING_FIELD(aliasname);
	WRITE_NODE_FIELD(colnames);
}

static void
_outRangeTblEntry(StringInfo str, RangeTblEntry *node)
{
	WRITE_NODE_TYPE("RTE");

	/* put alias + eref first to make dump more legible */
	WRITE_NODE_FIELD(alias);
	WRITE_NODE_FIELD(eref);
	WRITE_ENUM_FIELD(rtekind, RTEKind);

	switch (node->rtekind)
	{
		case RTE_RELATION:
		case RTE_SPECIAL:
			WRITE_OID_FIELD(relid);
			break;
		case RTE_SUBQUERY:
			WRITE_NODE_FIELD(subquery);
			break;
		case RTE_FUNCTION:
			WRITE_NODE_FIELD(funcexpr);
			WRITE_NODE_FIELD(coldeflist);
			break;
		case RTE_JOIN:
			WRITE_ENUM_FIELD(jointype, JoinType);
			WRITE_NODE_FIELD(joinaliasvars);
			break;
		default:
			elog(ERROR, "bogus rte kind %d", (int) node->rtekind);
			break;
	}

	WRITE_BOOL_FIELD(inh);
	WRITE_BOOL_FIELD(inFromCl);
	WRITE_BOOL_FIELD(checkForRead);
	WRITE_BOOL_FIELD(checkForWrite);
	WRITE_OID_FIELD(checkAsUser);
}

/*
 * print the basic stuff of all nodes that inherit from Path
 */
static void
_outPathInfo(StringInfo str, Path *node)
{
	WRITE_ENUM_FIELD(pathtype, NodeTag);
	WRITE_FLOAT_FIELD(startup_cost, "%.2f");
	WRITE_FLOAT_FIELD(total_cost, "%.2f");
	WRITE_NODE_FIELD(pathkeys);
}

/*
 * print the basic stuff of all nodes that inherit from JoinPath
 */
static void
_outJoinPathInfo(StringInfo str, JoinPath *node)
{
	_outPathInfo(str, (Path *) node);

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_NODE_FIELD(outerjoinpath);
	WRITE_NODE_FIELD(innerjoinpath);
	WRITE_NODE_FIELD(joinrestrictinfo);
}

static void
_outPath(StringInfo str, Path *node)
{
	WRITE_NODE_TYPE("PATH");

	_outPathInfo(str, (Path *) node);
}

/*
 *	IndexPath is a subclass of Path.
 */
static void
_outIndexPath(StringInfo str, IndexPath *node)
{
	WRITE_NODE_TYPE("INDEXPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(indexinfo);
	WRITE_NODE_FIELD(indexqual);
	WRITE_ENUM_FIELD(indexscandir, ScanDirection);
	WRITE_FLOAT_FIELD(rows, "%.2f");
}

static void
_outTidPath(StringInfo str, TidPath *node)
{
	WRITE_NODE_TYPE("TIDPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(tideval);
	WRITE_INTLIST_FIELD(unjoined_relids);
}

static void
_outAppendPath(StringInfo str, AppendPath *node)
{
	WRITE_NODE_TYPE("APPENDPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(subpaths);
}

static void
_outResultPath(StringInfo str, ResultPath *node)
{
	WRITE_NODE_TYPE("RESULTPATH");

	_outPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(subpath);
	WRITE_NODE_FIELD(constantqual);
}

static void
_outNestPath(StringInfo str, NestPath *node)
{
	WRITE_NODE_TYPE("NESTPATH");

	_outJoinPathInfo(str, (JoinPath *) node);
}

static void
_outMergePath(StringInfo str, MergePath *node)
{
	WRITE_NODE_TYPE("MERGEPATH");

	_outJoinPathInfo(str, (JoinPath *) node);

	WRITE_NODE_FIELD(path_mergeclauses);
	WRITE_NODE_FIELD(outersortkeys);
	WRITE_NODE_FIELD(innersortkeys);
}

static void
_outHashPath(StringInfo str, HashPath *node)
{
	WRITE_NODE_TYPE("HASHPATH");

	_outJoinPathInfo(str, (JoinPath *) node);

	WRITE_NODE_FIELD(path_hashclauses);
}

static void
_outPathKeyItem(StringInfo str, PathKeyItem *node)
{
	WRITE_NODE_TYPE("PATHKEYITEM");

	WRITE_NODE_FIELD(key);
	WRITE_OID_FIELD(sortop);
}

static void
_outRestrictInfo(StringInfo str, RestrictInfo *node)
{
	WRITE_NODE_TYPE("RESTRICTINFO");

	WRITE_NODE_FIELD(clause);
	WRITE_BOOL_FIELD(ispusheddown);
	WRITE_NODE_FIELD(subclauseindices);
	WRITE_OID_FIELD(mergejoinoperator);
	WRITE_OID_FIELD(left_sortop);
	WRITE_OID_FIELD(right_sortop);
	WRITE_OID_FIELD(hashjoinoperator);
}

static void
_outJoinInfo(StringInfo str, JoinInfo *node)
{
	WRITE_NODE_TYPE("JOININFO");

	WRITE_INTLIST_FIELD(unjoined_relids);
	WRITE_NODE_FIELD(jinfo_restrictinfo);
}

/*
 * Print the value of a Datum given its type.
 */
static void
_outDatum(StringInfo str, Datum value, int typlen, bool typbyval)
{
	Size		length,
				i;
	char	   *s;

	length = datumGetSize(value, typbyval, typlen);

	if (typbyval)
	{
		s = (char *) (&value);
		appendStringInfo(str, "%u [ ", (unsigned int) length);
		for (i = 0; i < (Size) sizeof(Datum); i++)
			appendStringInfo(str, "%d ", (int) (s[i]));
		appendStringInfo(str, "]");
	}
	else
	{
		s = (char *) DatumGetPointer(value);
		if (!PointerIsValid(s))
			appendStringInfo(str, "0 [ ]");
		else
		{
			appendStringInfo(str, "%u [ ", (unsigned int) length);
			for (i = 0; i < length; i++)
				appendStringInfo(str, "%d ", (int) (s[i]));
			appendStringInfo(str, "]");
		}
	}
}

static void
_outAExpr(StringInfo str, A_Expr *node)
{
	WRITE_NODE_TYPE("AEXPR");

	switch (node->oper)
	{
		case AND:
			appendStringInfo(str, " AND");
			break;
		case OR:
			appendStringInfo(str, " OR");
			break;
		case NOT:
			appendStringInfo(str, " NOT");
			break;
		case OP:
			appendStringInfo(str, " ");
			WRITE_NODE_FIELD(name);
			break;
		default:
			appendStringInfo(str, " ??");
			break;
	}

	WRITE_NODE_FIELD(lexpr);
	WRITE_NODE_FIELD(rexpr);
}

static void
_outValue(StringInfo str, Value *value)
{
	switch (value->type)
	{
		case T_Integer:
			appendStringInfo(str, "%ld", value->val.ival);
			break;
		case T_Float:

			/*
			 * We assume the value is a valid numeric literal and so does
			 * not need quoting.
			 */
			appendStringInfo(str, "%s", value->val.str);
			break;
		case T_String:
			appendStringInfoChar(str, '"');
			_outToken(str, value->val.str);
			appendStringInfoChar(str, '"');
			break;
		case T_BitString:
			/* internal representation already has leading 'b' */
			appendStringInfo(str, "%s", value->val.str);
			break;
		default:
			elog(WARNING, "_outValue: don't know how to print type %d",
				 value->type);
			break;
	}
}

static void
_outRangeVar(StringInfo str, RangeVar *node)
{
	WRITE_NODE_TYPE("RANGEVAR");

	/*
	 * we deliberately ignore catalogname here, since it is presently not
	 * semantically meaningful
	 */
	WRITE_STRING_FIELD(schemaname);
	WRITE_STRING_FIELD(relname);
	WRITE_ENUM_FIELD(inhOpt, InhOption);
	WRITE_BOOL_FIELD(istemp);
	WRITE_NODE_FIELD(alias);
}

static void
_outColumnRef(StringInfo str, ColumnRef *node)
{
	WRITE_NODE_TYPE("COLUMNREF");

	WRITE_NODE_FIELD(fields);
	WRITE_NODE_FIELD(indirection);
}

static void
_outParamRef(StringInfo str, ParamRef *node)
{
	WRITE_NODE_TYPE("PARAMREF");

	WRITE_INT_FIELD(number);
	WRITE_NODE_FIELD(fields);
	WRITE_NODE_FIELD(indirection);
}

static void
_outAConst(StringInfo str, A_Const *node)
{
	WRITE_NODE_TYPE("CONST ");

	_outValue(str, &(node->val));
	WRITE_NODE_FIELD(typename);
}

static void
_outExprFieldSelect(StringInfo str, ExprFieldSelect *node)
{
	WRITE_NODE_TYPE("EXPRFIELDSELECT");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(fields);
	WRITE_NODE_FIELD(indirection);
}

static void
_outConstraint(StringInfo str, Constraint *node)
{
	WRITE_NODE_TYPE("CONSTRAINT");

	WRITE_STRING_FIELD(name);

	appendStringInfo(str, " :contype ");
	switch (node->contype)
	{
		case CONSTR_PRIMARY:
			appendStringInfo(str, "PRIMARY_KEY");
			WRITE_NODE_FIELD(keys);
			break;

		case CONSTR_CHECK:
			appendStringInfo(str, "CHECK");
			WRITE_NODE_FIELD(raw_expr);
			WRITE_STRING_FIELD(cooked_expr);
			break;

		case CONSTR_DEFAULT:
			appendStringInfo(str, "DEFAULT");
			WRITE_NODE_FIELD(raw_expr);
			WRITE_STRING_FIELD(cooked_expr);
			break;

		case CONSTR_NOTNULL:
			appendStringInfo(str, "NOT_NULL");
			break;

		case CONSTR_UNIQUE:
			appendStringInfo(str, "UNIQUE");
			WRITE_NODE_FIELD(keys);
			break;

		default:
			appendStringInfo(str, "<unrecognized_constraint>");
			break;
	}
}

static void
_outFkConstraint(StringInfo str, FkConstraint *node)
{
	WRITE_NODE_TYPE("FKCONSTRAINT");

	WRITE_STRING_FIELD(constr_name);
	WRITE_NODE_FIELD(pktable);
	WRITE_NODE_FIELD(fk_attrs);
	WRITE_NODE_FIELD(pk_attrs);
	WRITE_CHAR_FIELD(fk_matchtype);
	WRITE_CHAR_FIELD(fk_upd_action);
	WRITE_CHAR_FIELD(fk_del_action);
	WRITE_BOOL_FIELD(deferrable);
	WRITE_BOOL_FIELD(initdeferred);
	WRITE_BOOL_FIELD(skip_validation);
}

static void
_outCaseExpr(StringInfo str, CaseExpr *node)
{
	WRITE_NODE_TYPE("CASE");

	WRITE_OID_FIELD(casetype);
	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(defresult);
}

static void
_outCaseWhen(StringInfo str, CaseWhen *node)
{
	WRITE_NODE_TYPE("WHEN");

	WRITE_NODE_FIELD(expr);
	WRITE_NODE_FIELD(result);
}

static void
_outNullTest(StringInfo str, NullTest *node)
{
	WRITE_NODE_TYPE("NULLTEST");

	WRITE_NODE_FIELD(arg);
	WRITE_ENUM_FIELD(nulltesttype, NullTestType);
}

static void
_outBooleanTest(StringInfo str, BooleanTest *node)
{
	WRITE_NODE_TYPE("BOOLEANTEST");

	WRITE_NODE_FIELD(arg);
	WRITE_ENUM_FIELD(booltesttype, BoolTestType);
}

static void
_outConstraintTest(StringInfo str, ConstraintTest *node)
{
	WRITE_NODE_TYPE("CONSTRAINTTEST");

	WRITE_NODE_FIELD(arg);
	WRITE_ENUM_FIELD(testtype, ConstraintTestType);
	WRITE_STRING_FIELD(name);
	WRITE_STRING_FIELD(domname);
	WRITE_NODE_FIELD(check_expr);
}

static void
_outDomainConstraintValue(StringInfo str, DomainConstraintValue *node)
{
	WRITE_NODE_TYPE("DOMAINCONSTRAINTVALUE");
}

static void
_outConstraintTestValue(StringInfo str, ConstraintTestValue *node)
{
	WRITE_NODE_TYPE("CONSTRAINTTESTVALUE");

	WRITE_OID_FIELD(typeId);
	WRITE_INT_FIELD(typeMod);
}


/*
 * _outNode -
 *	  converts a Node into ascii string and append it to 'str'
 */
static void
_outNode(StringInfo str, void *obj)
{
	if (obj == NULL)
	{
		appendStringInfo(str, "<>");
		return;
	}

	if (IsA(obj, List))
	{
		List	   *l;

		appendStringInfoChar(str, '(');
		foreach(l, (List *) obj)
		{
			_outNode(str, lfirst(l));
			if (lnext(l))
				appendStringInfoChar(str, ' ');
		}
		appendStringInfoChar(str, ')');
	}
	else if (IsA(obj, Integer) ||
			 IsA(obj, Float) ||
			 IsA(obj, String) ||
			 IsA(obj, BitString))
	{
		/* nodeRead does not want to see { } around these! */
		_outValue(str, obj);
	}
	else
	{
		appendStringInfoChar(str, '{');
		switch (nodeTag(obj))
		{
			case T_CreateStmt:
				_outCreateStmt(str, obj);
				break;
			case T_IndexStmt:
				_outIndexStmt(str, obj);
				break;
			case T_NotifyStmt:
				_outNotifyStmt(str, obj);
				break;
			case T_SelectStmt:
				_outSelectStmt(str, obj);
				break;
			case T_ColumnDef:
				_outColumnDef(str, obj);
				break;
			case T_TypeName:
				_outTypeName(str, obj);
				break;
			case T_TypeCast:
				_outTypeCast(str, obj);
				break;
			case T_IndexElem:
				_outIndexElem(str, obj);
				break;
			case T_Query:
				_outQuery(str, obj);
				break;
			case T_SortClause:
				_outSortClause(str, obj);
				break;
			case T_GroupClause:
				_outGroupClause(str, obj);
				break;
			case T_SetOperationStmt:
				_outSetOperationStmt(str, obj);
				break;
			case T_Plan:
				_outPlan(str, obj);
				break;
			case T_Result:
				_outResult(str, obj);
				break;
			case T_Append:
				_outAppend(str, obj);
				break;
			case T_Join:
				_outJoin(str, obj);
				break;
			case T_NestLoop:
				_outNestLoop(str, obj);
				break;
			case T_MergeJoin:
				_outMergeJoin(str, obj);
				break;
			case T_HashJoin:
				_outHashJoin(str, obj);
				break;
			case T_Scan:
				_outScan(str, obj);
				break;
			case T_SeqScan:
				_outSeqScan(str, obj);
				break;
			case T_IndexScan:
				_outIndexScan(str, obj);
				break;
			case T_TidScan:
				_outTidScan(str, obj);
				break;
			case T_SubqueryScan:
				_outSubqueryScan(str, obj);
				break;
			case T_FunctionScan:
				_outFunctionScan(str, obj);
				break;
			case T_Material:
				_outMaterial(str, obj);
				break;
			case T_Sort:
				_outSort(str, obj);
				break;
			case T_Agg:
				_outAgg(str, obj);
				break;
			case T_Group:
				_outGroup(str, obj);
				break;
			case T_Unique:
				_outUnique(str, obj);
				break;
			case T_SetOp:
				_outSetOp(str, obj);
				break;
			case T_Limit:
				_outLimit(str, obj);
				break;
			case T_Hash:
				_outHash(str, obj);
				break;
			case T_SubPlan:
				_outSubPlan(str, obj);
				break;
			case T_Resdom:
				_outResdom(str, obj);
				break;
			case T_Expr:
				_outExpr(str, obj);
				break;
			case T_Var:
				_outVar(str, obj);
				break;
			case T_Const:
				_outConst(str, obj);
				break;
			case T_Aggref:
				_outAggref(str, obj);
				break;
			case T_SubLink:
				_outSubLink(str, obj);
				break;
			case T_ArrayRef:
				_outArrayRef(str, obj);
				break;
			case T_Func:
				_outFunc(str, obj);
				break;
			case T_Oper:
				_outOper(str, obj);
				break;
			case T_Param:
				_outParam(str, obj);
				break;
			case T_FieldSelect:
				_outFieldSelect(str, obj);
				break;
			case T_RelabelType:
				_outRelabelType(str, obj);
				break;
			case T_RangeTblRef:
				_outRangeTblRef(str, obj);
				break;
			case T_FromExpr:
				_outFromExpr(str, obj);
				break;
			case T_JoinExpr:
				_outJoinExpr(str, obj);
				break;
			case T_TargetEntry:
				_outTargetEntry(str, obj);
				break;
			case T_Alias:
				_outAlias(str, obj);
				break;
			case T_RangeTblEntry:
				_outRangeTblEntry(str, obj);
				break;
			case T_Path:
				_outPath(str, obj);
				break;
			case T_IndexPath:
				_outIndexPath(str, obj);
				break;
			case T_TidPath:
				_outTidPath(str, obj);
				break;
			case T_AppendPath:
				_outAppendPath(str, obj);
				break;
			case T_ResultPath:
				_outResultPath(str, obj);
				break;
			case T_NestPath:
				_outNestPath(str, obj);
				break;
			case T_MergePath:
				_outMergePath(str, obj);
				break;
			case T_HashPath:
				_outHashPath(str, obj);
				break;
			case T_PathKeyItem:
				_outPathKeyItem(str, obj);
				break;
			case T_RestrictInfo:
				_outRestrictInfo(str, obj);
				break;
			case T_JoinInfo:
				_outJoinInfo(str, obj);
				break;
			case T_A_Expr:
				_outAExpr(str, obj);
				break;
			case T_RangeVar:
				_outRangeVar(str, obj);
				break;
			case T_ColumnRef:
				_outColumnRef(str, obj);
				break;
			case T_ParamRef:
				_outParamRef(str, obj);
				break;
			case T_A_Const:
				_outAConst(str, obj);
				break;
			case T_ExprFieldSelect:
				_outExprFieldSelect(str, obj);
				break;
			case T_Constraint:
				_outConstraint(str, obj);
				break;
			case T_FkConstraint:
				_outFkConstraint(str, obj);
				break;
			case T_CaseExpr:
				_outCaseExpr(str, obj);
				break;
			case T_CaseWhen:
				_outCaseWhen(str, obj);
				break;
			case T_NullTest:
				_outNullTest(str, obj);
				break;
			case T_BooleanTest:
				_outBooleanTest(str, obj);
				break;
			case T_ConstraintTest:
				_outConstraintTest(str, obj);
				break;
			case T_ConstraintTestValue:
				_outConstraintTestValue(str, obj);
				break;
			case T_FuncCall:
				_outFuncCall(str, obj);
				break;
			case T_DomainConstraintValue:
				_outDomainConstraintValue(str, obj);
				break;

			default:
				elog(WARNING, "_outNode: don't know how to print type %d",
					 nodeTag(obj));
				break;
		}
		appendStringInfoChar(str, '}');
	}
}

/*
 * nodeToString -
 *	   returns the ascii representation of the Node as a palloc'd string
 */
char *
nodeToString(void *obj)
{
	StringInfoData str;

	/* see stringinfo.h for an explanation of this maneuver */
	initStringInfo(&str);
	_outNode(&str, obj);
	return str.data;
}
