#include "parser.h"

#include <assert.h>

static inline int do_fparam_binds(Proc* proc) {
	FParam* fparam = proc->first_fparam;
	Context* ctx = &proc->ctx;
	while (fparam) {
		if (local_lookup(ctx, fparam->id))
			return 0;
		if (!bind_fparam(ctx, fparam->id, fparam))
			return 0;

		fparam->type_bind = lookup(ctx, fparam->type);
		if (!fparam->type_bind)
			return 0;

		fparam = fparam->next;
	}
	return 1;
}

static inline int do_var_binds(Proc* proc) {
	Var* var = proc->first_var;
	Context* ctx = &proc->ctx;
	while (var) {
		if (local_lookup(ctx, var->id))
			return 0;
		if (!bind_var(ctx, var->id, var))
			return 0;

		var->type_bind = lookup(ctx, var->type);
		if (!var->type_bind)
			return 0;

		var = var->next;
	}
	return 1;
}

static int resolve_expr(Context* ctx, Expr* expr);

static inline int resolve_bin_expr(Context* ctx, BinaryExpr* bin) {
	return resolve_expr(ctx, bin->left)
		&& resolve_expr(ctx, bin->right);
}

static inline int resolve_id_expr(Context* ctx, IdExpr* id) {
	id->bind = lookup(ctx, id->name);
	if (!id->bind)
		return 0;
	return 1;
}

static inline int resolve_call_expr(Context* ctx, CallExpr* call) {
	int ok = resolve_expr(ctx, call->lvalue);
	if (ok) {
		Param *param = call->first_param;
		while (ok && param) {
			ok = resolve_expr(ctx, param->value);
			if (ok)
				param = param->next;
		}
	}
	return ok;
}

int resolve_expr(Context* ctx, Expr* expr) {
	switch (expr->type) {
	case EXPR_BINARY:
		return resolve_bin_expr(ctx, expr->content.as_binary);
	case EXPR_ID:
		return resolve_id_expr(ctx, expr->content.as_id);
	case EXPR_NUM:
		return 1;
	case EXPR_CALL:
		return resolve_call_expr(ctx, expr->content.as_call);
	}
	assert(0);
}

static int resolve_stmts(Context* ctx, Stmt* stmt);

static inline int resolve_assign(Context* ctx, AssignStmt* assign) {
	return resolve_expr(ctx, assign->lvalue)
		&& resolve_expr(ctx, assign->rvalue);
}

static inline int resolve_for(Context* ctx, ForStmt* forstmt) {
	forstmt->bind = lookup(ctx, forstmt->id);
	if (!forstmt->bind)
		return 0;
	return resolve_expr(ctx, forstmt->from)
		&& resolve_expr(ctx, forstmt->to)
		&& resolve_stmts(ctx, forstmt->first_stmt);
}

static inline int resolve_return(Context* ctx, ReturnStmt* ret) {
	if (ret->expr)
		return resolve_expr(ctx, ret->expr);
	return 1;
}

static inline int resolve_call(Context* ctx, CallStmt* call) {
	return resolve_expr(ctx, call->expr);
}

int resolve_stmts(Context* ctx, Stmt* stmt) {
	int ok = 1;
	while (ok && stmt) {
		switch (stmt->type) {
		case STMT_ASSIGN:
			ok = resolve_assign(ctx, stmt->content.as_assign);
			break;
		case STMT_FOR:
			ok = resolve_for(ctx, stmt->content.as_for);
			break;
		case STMT_RETURN:
			ok = resolve_return(ctx, stmt->content.as_return);
			break;
		case STMT_CALL:
			ok = resolve_call(ctx, stmt->content.as_call);
			break;
		default:
			ok = 0;
			assert(0);
		}
		if (ok)
			stmt = stmt->next;
	}
	return ok;
}

static inline int do_return_bind(Proc* proc) {
	if (!proc->is_function)
		return 0;

	proc->return_type_bind = lookup(&proc->ctx, proc->return_type);
	return proc->return_type_bind ? 1 : 0;
}

static inline int resolve_proc_binds(Proc* proc) {
	return do_return_bind(proc)
		&& do_fparam_binds(proc) 
		&& do_var_binds(proc)
		&& resolve_stmts(&proc->ctx, proc->first_stmt);
}

static inline int do_global_binds(Prog* prog) {
	Proc* proc = prog->first_proc;
	Context* ctx = &prog->ctx;
	while (proc) {
		char* id = proc->id;
		if (local_lookup(ctx, id))
			return 0;
		if (!bind_proc(ctx, id, proc))
			return 0;
		proc = proc->next;
	}
	return 1;
}

int resolve_binds(Prog* prog) {
	int ok = do_global_binds(prog);
	if (ok) {
		Proc* proc = prog->first_proc;
		while (ok && proc) {
			ok = resolve_proc_binds(proc);
			proc = proc->next;
		}
	}
	return ok;
}