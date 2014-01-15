#include "parser.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct InstrNode InstrNode;

struct InstrNode {
	InterpInstr   instr;
	InstrNode*    prev;
};

typedef struct CompileCtx {
	InstrNode*  last;
	int         pc;
} CompileCtx;

static inline InstrNode* append_instr(CompileCtx* ctx) {
	InstrNode* n = (InstrNode*) calloc(1, sizeof(InstrNode));
	if (n) {
		n->prev = ctx->last;
		ctx->pc++;
		ctx->last = n;
	}
	return n;
}

static int compile_expr(CompileCtx* ctx, Expr* expr, int rvalue);

static inline int compile_bin_expr(CompileCtx* ctx, BinaryExpr *bin) {
	InstrNode* n;
	if (!compile_expr(ctx, bin->left, 1) || !compile_expr(ctx, bin->right, 1))
		return 0;
	n = append_instr(ctx);
	if (!n)
		return 0;
	switch (bin->op) {
	case OP_ADD:
		n->instr.op = INTERP_ADD;
		break;
	case OP_MULT:
		n->instr.op = INTERP_MUL;
		break;
	default:
		assert(0);
	}
	return 1;
}

static inline int compile_id_expr(CompileCtx* ctx, IdExpr *id, int rvalue) {
	int ok = 0;
	switch (id->bind->type) {
	case BIND_FPARAM: {
		InstrNode* n = append_instr(ctx);
		if (n) {
			n->instr.op = INTERP_PARAM;
			n->instr.value = id->bind->content.as_fparam->nid;
			ok = 1;
		}
		break;
	}
	case BIND_VAR: {
		InstrNode* n = append_instr(ctx);
		if (n) {
			n->instr.op = INTERP_VAR;
			n->instr.value = id->bind->content.as_var->nid;
			ok = 1;
		}
		break;
	}
	case BIND_PROC: {
		InstrNode* n = append_instr(ctx);
		if (n) {
			n->instr.op = INTERP_PROC;
			n->instr.value = id->bind->content.as_proc->nid;
			ok = 1;
		}
		break;
	}
	default:
		assert(0);
	}
	if (ok && rvalue) {
		InstrNode* n = append_instr(ctx);
		if (n)
			n->instr.op = INTERP_LOAD;
	}
	return ok;
}

static inline int compile_num_expr(CompileCtx* ctx, NumExpr *num) {
	InstrNode* instr = append_instr(ctx);
	if (!instr)
		return 0;
	instr->instr.op = INTERP_PUSH;
	instr->instr.value = num->value;
	return 1;
}

static int compile_params(CompileCtx* ctx, Param* param) {
	int ok;
	if (!param)
		return 1;
	ok = param->next ? compile_params(ctx, param->next) : 1;
	if (ok)
		ok = compile_expr(ctx, param->value, 1);
	return ok;
}

static inline int compile_call_expr(CompileCtx* ctx, CallExpr* call) {
	Type* type = call->lvalue->actual_type.type;

	assert(type->kind == TYPE_PROC);

	int ok = compile_params(ctx, call->first_param);
	if (ok) {
		ok = compile_expr(ctx, call->lvalue, 0);
		if (ok) {
			InstrNode* n = append_instr(ctx);
			if (n)
				n->instr.op = type->content.as_proc->return_type 
					? INTERP_CALL : INTERP_CALLV;
			else
				ok = 0;
		}
	}
	return ok;
}

int compile_expr(CompileCtx* ctx, Expr* expr, int rvalue) {
	switch (expr->type) {
	case EXPR_BINARY:
		return compile_bin_expr(ctx, expr->content.as_binary);
	case EXPR_ID:
		return compile_id_expr(ctx, expr->content.as_id, rvalue);
	case EXPR_NUM:
		return compile_num_expr(ctx, expr->content.as_num);
	case EXPR_CALL:
		return compile_call_expr(ctx, expr->content.as_call);
	default:
		assert(0);
	}
	return 0;
}

static int compile_stmt(CompileCtx* ctx, Stmt* stmt);

static inline 
int compile_assign_stmt(CompileCtx* ctx, AssignStmt* assign) {
	InstrNode *n;
	if (!compile_expr(ctx, assign->rvalue, 1) || !compile_expr(ctx, assign->lvalue, 0))
		return 0;
	n = append_instr(ctx);
	if (!n)
		return 0;
	n->instr.op = INTERP_STORE;
	return 1;
}

static inline  int compile_for_stmt(CompileCtx* ctx, ForStmt* forstmt) {
	int pc_head;
	InstrNode* njlt;
	int ok = compile_expr(ctx, forstmt->from, 1);
	if (ok) {
		InstrNode* n = append_instr(ctx);
		if (n) {
			assert(forstmt->bind->type == BIND_VAR);
			n->instr.op = INTERP_VAR;
			n->instr.value = forstmt->bind->content.as_var->nid;
		} else
			ok = 0;
	}
	if (ok) {
		InstrNode* n = append_instr(ctx);
		if (n)
			n->instr.op = INTERP_STORE;
		else
			ok = 0;
	}
	if (ok)
		ok = compile_expr(ctx, forstmt->to, 1);
	if (ok) {
		InstrNode* n;
		pc_head = ctx->pc;
		n = append_instr(ctx);
		if (n)
			n->instr.op = INTERP_DUP;
		else
			ok = 0;
	}
	if (ok) {
		InstrNode* n = append_instr(ctx);
		if (n) {
			n->instr.op = INTERP_VAR;
			n->instr.value = forstmt->bind->content.as_var->nid;
		} else
			ok = 0;
	}
	if (ok) {
		InstrNode* n = append_instr(ctx);
		if (n)
			n->instr.op = INTERP_LOAD;
		else
			ok = 0;
	}
	if (ok) {
		InstrNode* n = append_instr(ctx);
		if (n)
			n->instr.op = INTERP_CMP;
		else
			ok = 0;
	}
	if (ok) {
		InstrNode* n = append_instr(ctx);
		if (n) {
			n->instr.op = INTERP_JLT;
			njlt = n;
		} else
			ok = 0;
	}
	if (ok)
		ok = compile_stmt(ctx, forstmt->first_stmt);
	if (ok) {
		InstrNode* n = append_instr(ctx);
		if (n) {
			n->instr.op = INTERP_VAR;
			n->instr.value = forstmt->bind->content.as_var->nid;
		} else
			ok = 0;
	}
	if (ok) {
		InstrNode* n = append_instr(ctx);
		if (n)
			n->instr.op = INTERP_INC;
	}
	if (ok) {
		InstrNode* n = append_instr(ctx);
		if (n) {
			n->instr.op = INTERP_JMP;
			n->instr.value = pc_head;
		} else
			ok = 0;
	}
	if (ok) {
		InstrNode* n = append_instr(ctx);
		if (n) {
			njlt->instr.value = ctx->pc - 1;
			n->instr.op = INTERP_POP;
			n->instr.value = ctx->pc;
		} else
			ok = 0;
	}

	return ok;
}

static inline int compile_return_stmt(CompileCtx* ctx, ReturnStmt* ret) {
	Proc* proc = ret->proc;
	int ok = compile_expr(ctx, ret->expr, 1);
	if (ok) {
		InstrNode* n = append_instr(ctx);
		if (n) {
			n->instr.op = proc->is_function ? INTERP_RET : INTERP_RETV;
			n->instr.value = proc->fparam_count; 
		}
		else
			ok = 0;		
	}

	return ok;

}

static inline int compile_call_stmt(CompileCtx* ctx, CallStmt* call) {
	Type* t = call->expr->actual_type.type;
	assert(t->kind == TYPE_PROC);
	int ok = compile_expr(ctx, call->expr, 1);
	if (ok && t->content.as_proc->return_type) {
		InstrNode* n = append_instr(ctx);
		if (n)
			n->instr.op = INTERP_POP;
	}
	return ok;
}

int compile_stmt(CompileCtx* ctx, Stmt* stmt) {
	int ok = 1;
	while (ok && stmt) {
		switch (stmt->type) {
		case STMT_ASSIGN:
			ok = compile_assign_stmt(ctx, stmt->content.as_assign);
			break;
		case STMT_FOR:
			ok = compile_for_stmt(ctx, stmt->content.as_for);
			break;
		case STMT_RETURN:
			ok = compile_return_stmt(ctx, stmt->content.as_return);
			break;
		case STMT_CALL:
			ok = compile_call_stmt(ctx, stmt->content.as_call);
			break;
		default:
			assert(0);
			ok = 0;
		}
		if (ok)
			stmt = stmt->next;	
	}
	return ok;
}

static inline
int compile_vars(CompileCtx* ctx, Var* var) {
	int ok = 1;
	while (ok && var) {
		InstrNode* n = append_instr(ctx);
		if (n) {
			n->instr.op = INTERP_PUSH;
			n->instr.value = 0;
			var = var->next;
		} else
			ok = 0;
	}
	return ok;
}

static inline
int compile_proc(CompileCtx* ctx, Proc* proc) {
	int ok = compile_vars(ctx, proc->first_var);
	if (ok)
		ok = compile_stmt(ctx, proc->first_stmt);
	return ok;
}

static inline free_instrs(InstrNode* n) {
	while (n) {
		InstrNode *temp = n;
		n = n->prev;
		free(temp);
	}
}

int compile(Prog* prog) {
	Proc* proc;
	int ok = 0;

	proc = prog->first_proc;
	while (proc) {
		if (strcmp(proc->id, "main") == 0) {
			prog->interp.main = proc->nid;
			ok = 1;
		}
		proc = proc->next;
	}
	if (!ok)
		return 0;

	init_interp(&prog->interp, prog->proc_count);
	proc = prog->first_proc;
	while (ok && proc) {
		CompileCtx ctx = {
			.last = NULL,
			.pc = 0
		};

		ok = compile_proc(&ctx, proc);
		if (ok) {
			InterpCode* code = &prog->interp.procs[proc->nid];
			ok = init_interp_code(code, ctx.pc);
			if (ok) {
				int i;
				InstrNode* n = ctx.last;
				for (i=ctx.pc-1; i>=0; i--) {
					memcpy(&code->data[i], &n->instr, sizeof(InterpInstr));
					n = n->prev;
				}
				assert(!n);
			}
		}
		free_instrs(ctx.last);
		proc = proc->next;
	}

	return ok;
}