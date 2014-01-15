#include "parser.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct ParseCtx {
	Lexer     lexer;
	Context*  upper_context;
	Proc*     current_proc;
	int       move_next;
} ParseCtx;

Type INTEGER = { 
	.kind = TYPE_INTEGER,
	.content = { .as_proc = NULL }
};

static Bind INTEGER_BIND = {
	.id = "integer",
	.type = BIND_TYPE,
	.content = { .as_type = &INTEGER },
	.next = NULL
};

static Context BUILTIN = { 
	.upper_context = NULL,
	.first_bind = &INTEGER_BIND
};

static inline Token peek_token(ParseCtx *pctx) {
	if (pctx->move_next) {
		pctx->move_next = 0;
		lex(&pctx->lexer);
	}
	return pctx->lexer.token;
}

static inline next_token(ParseCtx *pctx) {
	pctx->move_next = 1;
}

static void free_fparam(FParam *fparam) {
	if (!fparam)
		return;
	free_fparam(fparam->next);
	free(fparam);
}

static void free_var(Var *var) {
	if (!var)
		return;
	free_var(var->next);
	free(var);
}

static void free_expr(Expr* expr);

static inline void free_binary_expr(BinaryExpr* bin) {
	if (!bin)
		return;
	free_expr(bin->left);
	free_expr(bin->right);
	free(bin);
}

static inline void free_id_expr(IdExpr* id) {
	if (!id)
		return;
	free(id);
}

static inline void free_num_expr(NumExpr* num) {
	if (!num)
		return;
	free(num);
}

static void free_param(Param *param) {
	if (!param)
		return;
	free_expr(param->value);
	free_param(param->next);
	free(param);
}

static inline void free_call_expr(CallExpr* call) {
	if (!call)
		return;
	free_expr(call->lvalue);
	free_param(call->first_param);
	free(call);
}

void free_expr(Expr* expr) {
	if (!expr)
		return;
	switch (expr->type) {
	case EXPR_BINARY:
		free_binary_expr(expr->content.as_binary);
		break;
	case EXPR_ID:
		free_id_expr(expr->content.as_id);
		break;
	case EXPR_NUM:
		free_num_expr(expr->content.as_num);
		break;
	case EXPR_CALL:
		free_call_expr(expr->content.as_call);
		break;
	default:
		assert(0);
	}
	free(expr);
}

static void free_stmt(Stmt* stmt);

static inline void free_for(ForStmt* forstmt) {
	if (!forstmt)
		return;
	free_expr(forstmt->from);
	free_expr(forstmt->to);
	free_stmt(forstmt->first_stmt);
	free(forstmt);
}

static inline void free_return(ReturnStmt* ret) {
	if (!ret)
		return;
	if (ret->expr)
		free_expr(ret->expr);
	free(ret);
}

static inline void free_call(CallStmt* call) {
	if (!call)
		return;
	free_expr(call->expr);
	free(call);
}

static inline void free_assign(AssignStmt *assign) {
	if (!assign)
		return;
	free_expr(assign->lvalue);
	free_expr(assign->rvalue);
	free(assign);
}

void free_stmt(Stmt* stmt) {
	if (!stmt)
		return;
	switch (stmt->type) {
	case STMT_ASSIGN:
		free_assign(stmt->content.as_assign);
		break;
	case STMT_FOR:
		free_for(stmt->content.as_for);
		break;
	case STMT_RETURN:
		free_return(stmt->content.as_return);
		break;
	case STMT_CALL:
		free_call(stmt->content.as_call);
		break;
	default:
		assert(0);
	}
	if (stmt->next)
		free_stmt(stmt->next);
	free(stmt);
}

static void free_proc(Proc *proc) {
	if (!proc)
		return;
	destroy_type(&proc->type);
	free_fparam(proc->first_fparam);
	free_var(proc->first_var);
	free_stmt(proc->first_stmt);
	free_proc(proc->next);
	free_context(&proc->ctx);
	free(proc);
}

static ParseStatus parse_type(ParseCtx *pctx, char *id) {
	if (peek_token(pctx) != TK_ID)
		return PARSE_SYNTAX_ERROR;
	strcpy(id, &pctx->lexer.lexeme[0]);
	next_token(pctx);
	return PARSE_OK;
}

static ParseStatus parse_fparam(ParseCtx* pctx, FParam** first, FParam** last, int* fparam_count) {
	ParseStatus status;
	FParam* fst;
	FParam* lst;

	fst = (FParam*) calloc(1, sizeof(FParam));
	if (!fst)
		return PARSE_NO_MEM;

	if (peek_token(pctx) == TK_ID) {
		fst->nid = *fparam_count;
		strcpy(&fst->id[0], &pctx->lexer.lexeme[0]);
		next_token(pctx);
		status = PARSE_OK;
	} else
		status = PARSE_SYNTAX_ERROR;

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_COMMA) {
			FParam *f;
			next_token(pctx);
			status = parse_fparam(pctx, &f, &lst, fparam_count);
			if (status == PARSE_OK) {
				strcpy(&fst->type[0], &lst->type[0]);
				fst->next = f;
			}
		} else if (peek_token(pctx) == TK_COLON) {
			next_token(pctx);
			status = parse_type(pctx, &fst->type[0]);
			lst = fst;
		} else
			status = PARSE_SYNTAX_ERROR;		
	}

	if (status == PARSE_OK) {
		(*fparam_count)++;
		*first = fst;
		*last = lst;
	}
	else
		free_fparam(fst);
 	return PARSE_OK; 	
}

static ParseStatus parse_fparams(ParseCtx* pctx, FParam** result, int* fparam_count) {
	FParam* last;
	FParam* first;
	ParseStatus status;

	if (peek_token(pctx) != TK_LPAREN) {
		*result = NULL;
		return PARSE_OK;
	}
	next_token(pctx);

	first = last = NULL;
	status = PARSE_OK;
	if (peek_token(pctx) != TK_RPAREN) {
		while (status == PARSE_OK) {
			FParam* fst;
			FParam* lst;

			status = parse_fparam(pctx, &fst, &lst, fparam_count);
			if (status == PARSE_OK) {
				if (last)
					last->next = fst;
				if (!first)
					first = fst;
				last = lst;
				if (peek_token(pctx) == TK_SEMI)
					next_token(pctx);
				else
					break;
			}
		}
	}

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_RPAREN)
			next_token(pctx);
		else
			status = PARSE_SYNTAX_ERROR;
	}

	if (status == PARSE_OK)
		*result = first;
	else
		free_fparam(first);
	return status;
}

static ParseStatus parse_var(ParseCtx* pctx, int* var_count, Var** first, Var** last) {
	ParseStatus status;
	Var* fst;
	Var* lst;

	fst = (Var*)calloc(1, sizeof(Var));
	if (!fst)
		return PARSE_NO_MEM;

	if (peek_token(pctx) == TK_ID) {
		fst->nid = (*var_count)++;
		strcpy(&fst->id[0], &pctx->lexer.lexeme[0]);
		next_token(pctx);
		status = PARSE_OK;
	} else
		status = PARSE_SYNTAX_ERROR;

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_COMMA) {
			Var* f;
			next_token(pctx);
			status = parse_var(pctx, var_count, &f, &lst);
			if (status == PARSE_OK) {
				fst->next = f;
				strcpy(&fst->type[0], &lst->type[0]);
			}
		} else if (peek_token(pctx) == TK_COLON) {
			next_token(pctx);
			lst = fst;
			status = parse_type(pctx, &fst->type[0]);
			if (status == PARSE_OK) {
				if (peek_token(pctx) == TK_SEMI)
					next_token(pctx);
				else
				status = PARSE_SYNTAX_ERROR;
			}
		} else
			status = PARSE_SYNTAX_ERROR;
	}

	if (status == PARSE_OK) {
		*first = fst;
		*last = lst;
	}
	else
		free_var(fst);
	return status;
}

static ParseStatus parse_vars(ParseCtx *pctx, Var **result, int* var_count) {
	ParseStatus status;
	Var* first;
	Var* last;

	if (peek_token(pctx) != TK_VAR) {
		*result = NULL;
		return PARSE_OK;
	}
	next_token(pctx);

	first = last = NULL;
	do {
		Var* fst;
		Var* lst;
		status = parse_var(pctx, var_count, &fst, &lst);
		if (status == PARSE_OK) {
			if (!first)
				first = fst;
			if (last)
				last->next = fst;
			last = lst;
		}
	} while ((status == PARSE_OK) && (peek_token(pctx) == TK_ID));

	if (status != PARSE_OK) {
		if (first)
			free_var(first);
		return status;
	}

	*result = first;
	return PARSE_OK;
}

static ParseStatus parse_expr(ParseCtx* pctx, Expr** result);

static inline ParseStatus parse_id_expr(ParseCtx* pctx, IdExpr** result) {
	IdExpr *id;

	id = (IdExpr*) calloc(1, sizeof(IdExpr));
	if (!id)
		return PARSE_NO_MEM;
	strcpy(&id->name[0], &pctx->lexer.lexeme[0]);
	next_token(pctx);

	*result = id;
	return PARSE_OK;
}

static inline ParseStatus parse_num_expr(ParseCtx* pctx, NumExpr** result) {
	NumExpr *num;

	num = (NumExpr*) calloc(1, sizeof(NumExpr));
	if (!num)
		return PARSE_NO_MEM;
	num->value = atoi(&pctx->lexer.lexeme[0]);
	next_token(pctx);

	*result = num;
	return PARSE_OK;
}

static ParseStatus parse_atom_expr(ParseCtx* pctx, Expr** result) {
	Expr *expr;
	ParseStatus status;

	expr = NULL;
	if (peek_token(pctx) == TK_ID) {
		expr = (Expr*) calloc(1, sizeof(Expr));
		if (!expr)
			return PARSE_NO_MEM;
		expr->type = EXPR_ID;
		status = parse_id_expr(pctx, &expr->content.as_id);
	} else if (peek_token(pctx) == TK_NUM) {
		expr = (Expr*) calloc(1, sizeof(Expr));
		if (!expr)
			return PARSE_NO_MEM;
		expr->type = EXPR_NUM;
		status = parse_num_expr(pctx, &expr->content.as_num);
	} else if (peek_token(pctx) == TK_LPAREN) {
		next_token(pctx);
		status = parse_expr(pctx, &expr);
		if (status == PARSE_OK) {
			if (peek_token(pctx) == TK_RPAREN)
				next_token(pctx);
			else
				status = PARSE_SYNTAX_ERROR;
		}
	} else
		status = PARSE_SYNTAX_ERROR;

	if (status == PARSE_OK) 
		*result = expr;
	else if (expr)
		free_expr(expr);
	return status;
}

static ParseStatus parse_params(ParseCtx* pctx, Param** result) {
	ParseStatus status;
	Param* param;

	if (peek_token(pctx) == TK_RPAREN) {
		*result = NULL;
		return PARSE_OK;
	}

	param = (Param*) calloc(1, sizeof(Param));
	if (!param)
		return PARSE_NO_MEM;

	status = parse_expr(pctx, &param->value);
	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_COMMA) {
			Param* tail;

			next_token(pctx);
			status = parse_params(pctx, &tail);
			if (status == PARSE_OK)
				param->next = tail;
		}
	}

	if (status != PARSE_OK) {
		free_param(param);
		return status;
	}

	*result = param;
	return PARSE_OK;
}

static inline ParseStatus parse_call_expr(ParseCtx* pctx, Expr *lvalue, Expr** result) {
	ParseStatus status;
	Expr* expr;
	CallExpr* call;

	next_token(pctx);

	expr = (Expr*) calloc(1, sizeof(Expr));
	if (!expr)
		return PARSE_NO_MEM;
	expr->type = EXPR_CALL;
	call = (CallExpr*) calloc(1, sizeof(CallExpr));
	if (call) {
		expr->content.as_call = call;
		call->lvalue = lvalue;
		status = parse_params(pctx, &call->first_param);
		if (status == PARSE_OK) {
			if (peek_token(pctx) == TK_RPAREN)
				next_token(pctx);
			else
				status = PARSE_SYNTAX_ERROR;
		}
	} else {
		free(lvalue);
		status = PARSE_NO_MEM;
	}
 
	if (status == PARSE_OK)
		*result = expr;
	else
		free_expr(expr);
	return status;
}

static ParseStatus parse_single_expr(ParseCtx* pctx, Expr** result) {
	Expr* expr;
	ParseStatus status;

	status = parse_atom_expr(pctx, &expr);
	if ((status == PARSE_OK) && (peek_token(pctx) == TK_LPAREN))
		status = parse_call_expr(pctx, expr, &expr);

	if (status == PARSE_OK)
		*result = expr;
	return status;
}

static ParseStatus parse_mult_expr(ParseCtx* pctx, Expr** result) {
	Expr* expr;
	ParseStatus status;

	status = parse_single_expr(pctx, &expr);
	if (status != PARSE_OK)
		return status;

	while (peek_token(pctx) == TK_MULT) {
		Expr* mult;
		BinaryExpr *bin;

		next_token(pctx);
		mult = (Expr*) calloc(1, sizeof(Expr));
		if (!mult) {
			status = PARSE_NO_MEM;
			break;
		}
		mult->type = EXPR_BINARY;
		bin = (BinaryExpr*) calloc(1, sizeof(BinaryExpr));
		if (!bin) {
			free_expr(mult);
			status = PARSE_NO_MEM;
			break;
		}
		mult->content.as_binary = bin;

		status = parse_single_expr(pctx, &bin->right);
		if (status != PARSE_OK) {
			free_expr(mult);
			break;
		}

		bin->op = OP_MULT;
		bin->left = expr;
		expr = mult;
	}

	if (status != PARSE_OK) {
		free_expr(expr);
		return status;
	}

	*result = expr;
	return PARSE_OK;
}

static ParseStatus parse_add_expr(ParseCtx* pctx, Expr** result) {
	Expr *expr;
	ParseStatus status;

	status = parse_mult_expr(pctx, &expr);
	if (status != PARSE_OK)
		return status;

	while (peek_token(pctx) == TK_ADD) {
		Expr *add;
		BinaryExpr *bin;

		next_token(pctx);
		add = (Expr*) calloc(1, sizeof(Expr));
		if (!add) {
			status = PARSE_NO_MEM;
			break;
		}
		add->type = EXPR_BINARY;
		bin = (BinaryExpr*) calloc(1, sizeof(BinaryExpr));
		if (!bin) {
			free_expr(add);
			status = PARSE_NO_MEM;
			break;
		}
		add->content.as_binary = bin;

		status = parse_mult_expr(pctx, &bin->right);
		if (status != PARSE_OK) {
			free_expr(add);
			break;
		}

		bin->op = OP_ADD;
		bin->left = expr;
		expr = add;
	}

	if (status != PARSE_OK) {
		free_expr(expr);
		return status;
	}

	*result = expr;
	return PARSE_OK;
}

ParseStatus parse_expr(ParseCtx* pctx, Expr** result) {
	return parse_add_expr(pctx, result);
}

static ParseStatus parse_stmts(ParseCtx* pctx, Stmt** result);

static ParseStatus parse_for(ParseCtx* pctx, ForStmt** result) {
	ParseStatus status;
	ForStmt *forstmt;

	forstmt = (ForStmt*) calloc(1, sizeof(ForStmt));
	if (!forstmt)
		return PARSE_NO_MEM;

	next_token(pctx);
	if (peek_token(pctx) != TK_ID)
		return PARSE_SYNTAX_ERROR;
	strcpy(&forstmt->id[0], &pctx->lexer.lexeme[0]);
	next_token(pctx);

	status = PARSE_OK;
	if (peek_token(pctx) == TK_ASSIGN)
		next_token(pctx);
	else
		status = PARSE_SYNTAX_ERROR;

	if (status == PARSE_OK)
		status = parse_expr(pctx, &forstmt->from);

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_TO)
			next_token(pctx);
		else
			status = PARSE_SYNTAX_ERROR;
	}

	if (status == PARSE_OK)
		status = parse_expr(pctx, &forstmt->to);

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_DO)
			next_token(pctx);
		else
			status = PARSE_SYNTAX_ERROR;
	}

	if (status == PARSE_OK)
		status = parse_stmts(pctx, &forstmt->first_stmt);

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_DONE)
			next_token(pctx);
		else
			status = PARSE_SYNTAX_ERROR;
	}

	if (status != PARSE_OK) {
		free_for(forstmt);
		return status;
	}

	*result = forstmt;
	return PARSE_OK;
}

static ParseStatus parse_return(ParseCtx* pctx, ReturnStmt** result) {
	ReturnStmt *ret;

	next_token(pctx);
	ret = (ReturnStmt*) calloc(1, sizeof(ReturnStmt));
	if (!ret)
		return PARSE_NO_MEM;
	ret->proc = pctx->current_proc;
	ret->expr = NULL;

	if (peek_token(pctx) != TK_SEMI) {
		ParseStatus status = parse_expr(pctx, &ret->expr);
		if (status != PARSE_OK) {
			free_return(ret);
			return status;
		}		
	}

	*result = ret;
	return PARSE_OK;
}

static ParseStatus parse_assign(ParseCtx* pctx, Expr* lvalue, AssignStmt** result) {
	AssignStmt *assign;
	ParseStatus status;

	next_token(pctx);
	assign = (AssignStmt*) calloc(1, sizeof(AssignStmt));
	if (!assign)
		return PARSE_NO_MEM;

	assign->lvalue = lvalue;
	status = parse_expr(pctx, &assign->rvalue);
	if (status != PARSE_OK) {
		free_assign(assign);
		return status;
	}

	*result = assign;
	return PARSE_OK;
}

static ParseStatus parse_call(ParseCtx* pctx, Expr* lvalue, CallStmt** result) {
	CallStmt *call;

	call = (CallStmt*) calloc(1, sizeof(CallStmt));
	if (!call)
		return PARSE_NO_MEM;
	call->expr = lvalue;

	*result = call;
	return PARSE_OK;
}

ParseStatus parse_stmts(ParseCtx* pctx, Stmt** result) {
	ParseStatus status;
	Stmt* stmt;

	if ((peek_token(pctx) == TK_END) || (peek_token(pctx) == TK_DONE)) {
		*result = NULL;
		return PARSE_OK;
	}

	stmt = (Stmt*) calloc(1, sizeof(Stmt));
	if (!stmt)
		return PARSE_NO_MEM;

	if (peek_token(pctx) == TK_FOR) {
		stmt->type = STMT_FOR;
		status = parse_for(pctx, &stmt->content.as_for);
	} else if (peek_token(pctx) == TK_RETURN) {
		stmt->type = STMT_RETURN;
		status = parse_return(pctx, &stmt->content.as_return);
	} else {
		Expr* expr;
		status = parse_expr(pctx, &expr);
		if (status == PARSE_OK) {
			if (peek_token(pctx) == TK_ASSIGN) {
				stmt->type = STMT_ASSIGN;
				status = parse_assign(pctx, expr, &stmt->content.as_assign);
			} else {
				stmt->type = STMT_CALL;
				status = parse_call(pctx, expr, &stmt->content.as_call);
			}
			if (status != PARSE_OK)
				free_expr(expr);
		}
	}

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_SEMI)
			next_token(pctx);
		else
			status = PARSE_SYNTAX_ERROR;
	}

	if (status == PARSE_OK) {
		Stmt* tail;
		status = parse_stmts(pctx, &tail);
		if (status == PARSE_OK)
			stmt->next = tail;
	}

	if (status != PARSE_OK) {
		free_stmt(stmt);
		return status;
	}

	*result = stmt;
	return PARSE_OK;
}

static ParseStatus parse_proc(ParseCtx *pctx, int nid, Proc **result) {
	ParseStatus status;
	Proc* proc;
	Context* saved_context;

	if (peek_token(pctx) != TK_PROCEDURE)
		return PARSE_SYNTAX_ERROR;
	next_token(pctx);

	if (peek_token(pctx) != TK_ID)
		return PARSE_SYNTAX_ERROR;
	proc = (Proc*) calloc(1, sizeof(Proc));
	if (!proc)
		return PARSE_NO_MEM;
	proc->nid = nid;
	strcpy(&proc->id[0], &pctx->lexer.lexeme[0]);
	next_token(pctx);

	saved_context = pctx->upper_context;
	init_context(&proc->ctx, saved_context);
	pctx->upper_context = &proc->ctx;

	status = parse_fparams(pctx, &proc->first_fparam, &proc->fparam_count);

	if ((status == PARSE_OK) && (peek_token(pctx) == TK_COLON)) {
		next_token(pctx);
		proc->is_function = 1;
		status = parse_type(pctx, &proc->return_type[0]);
	}

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_SEMI)
			next_token(pctx);
		else
			status = PARSE_SYNTAX_ERROR;
	}

	if (status == PARSE_OK)
		status = parse_vars(pctx, &proc->first_var, &proc->var_count);

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_BEGIN) {
			next_token(pctx);
			pctx->current_proc = proc;
		}
		else
			status = PARSE_SYNTAX_ERROR;
	}

	status = parse_stmts(pctx, &proc->first_stmt);

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_END)
			next_token(pctx);
		else
			status = PARSE_SYNTAX_ERROR;
	}

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_ID) {
			if (strcmp(&proc->id[0], &pctx->lexer.lexeme[0]) == 0)
				proc->mismatch = 1;
			next_token(pctx);
		}
		else
			status = PARSE_SYNTAX_ERROR;
	}

	if (status == PARSE_OK) {
		if (peek_token(pctx) == TK_SEMI)
			next_token(pctx);
		else
			status = PARSE_SYNTAX_ERROR;
	}

	pctx->upper_context = saved_context;
	if (status == PARSE_OK)
		*result = proc;
	else
		free_proc(proc);
	return status;
}

static ParseStatus parse_procs(ParseCtx* pctx, Proc** result, int* proc_count) {
	Proc* proc;
	ParseStatus status;

	if (peek_token(pctx) == TK_EOF) {
		*result = NULL;
		return PARSE_OK;
	}

	status = parse_proc(pctx, *proc_count, &proc);
	if (status == PARSE_OK) {
		Proc* tail;

		(*proc_count)++;
		status = parse_procs(pctx, &tail, proc_count);
		if (status == PARSE_OK) {
			proc->next = tail;
			*result = proc;
		}
		else
			free_proc(proc);
	}

	return status;
}

static ParseStatus parse_prog(ParseCtx* pctx, Prog** result) {
	Prog* prog;
	ParseStatus status;
	Context* saved_context;

	prog = (Prog*) calloc(1, sizeof(Prog));
	if (!prog)
		return PARSE_NO_MEM;
	saved_context = pctx->upper_context;
	init_context(&prog->ctx, saved_context);
	pctx->upper_context = &prog->ctx;

	status = parse_procs(pctx, &prog->first_proc, &prog->proc_count);

	pctx->upper_context = saved_context;
	if (status == PARSE_OK)
		*result = prog;
	else
		free_prog(prog);

	return status;
}

void init_parser(ParseCtx *pctx, GetChar input_fun, void* user_data) {
	init_lexer(&pctx->lexer, input_fun, user_data);
	pctx->upper_context = &BUILTIN;
	pctx->current_proc = NULL;
	next_token(pctx);
}

ParseStatus parse(GetChar input_fun, void* user_data, Prog **result) {
	ParseCtx pctx;
	init_parser(&pctx, input_fun, user_data);
	return parse_prog(&pctx, result);
}

void free_prog(Prog *prog) {
	assert(prog);

	free_proc(prog->first_proc);
	free_context(&prog->ctx);
	destroy_interp(&prog->interp);

	free(prog);
}