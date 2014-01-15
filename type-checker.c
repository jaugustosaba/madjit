#include "parser.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


static inline void free_proc_type(ProcType* proc_type) {
	free(proc_type->fparams);
	free(proc_type);
}

static inline int init_proc_type_fps(ProcType* proc_type, Proc* proc) {
	int i;
	int n = proc->fparam_count;
	FParam* fparam = proc->first_fparam;

	proc_type->fparams_count = n;
	proc_type->fparams = (Type**) calloc(n, sizeof(Type*));
	if (!proc_type->fparams)
		return 0;

	for (i=0; i<n; i++) {
		assert(fparam);
		proc_type->fparams[i] = fparam->actual_type.type;
		fparam = fparam->next;
	}

	return 1;
}

static inline int init_proc_type(Proc* proc) {
	ProcType *proc_type = (ProcType*) calloc(1, sizeof(ProcType));
	if (!proc_type)
		return 0;

	if (!init_proc_type_fps(proc_type, proc)) {
		free_proc_type(proc_type);
		return 0;		
	}

	if (proc->is_function)
		proc_type->return_type = proc->actual_return_type.type;

	proc->type.kind = TYPE_PROC;
	proc->type.content.as_proc = proc_type;

	proc->actual_type.type = &proc->type;
	proc->actual_type.lvalue = 1;
	proc->actual_type.constant = 1;

	return 1;
}

static inline int attach_types_fps(Proc* proc) {
	FParam* fparam = proc->first_fparam;
	while (fparam) {
		if (fparam->type_bind->type != BIND_TYPE)
			return 0;

		fparam->actual_type.type = fparam->type_bind->content.as_type;
		fparam->actual_type.lvalue = 1;
		fparam->actual_type.constant = 0;

		fparam = fparam->next;
	}
	return 1;
}

static inline int attach_types_vars(Proc* proc) {
	Var* var = proc->first_var;
	while (var) {
		if (var->type_bind->type != BIND_TYPE)
			return 0;

		var->actual_type.type = var->type_bind->content.as_type;
		var->actual_type.lvalue = 1;
		var->actual_type.constant = 0;

		var = var->next;
	}
	return 1;
}

static inline int attach_return_type(Proc* proc) {
	if (proc->return_type_bind->type != BIND_TYPE)
		return 0;
	proc->actual_return_type.type = proc->return_type_bind->content.as_type;
	proc->actual_return_type.lvalue = 0;
	proc->actual_return_type.constant = 0;
	return 1;
}

static inline int attach_types(Prog* prog) {
	int ok = 1;
	Proc* proc = prog->first_proc;
	while (ok && proc) {
		if (attach_types_fps(proc) 
			&& attach_types_vars(proc) 
			&& attach_return_type(proc) 
			&& init_proc_type(proc))
			proc = proc->next;
		else
			ok = 0;
	}
	return ok;
}

static int type_check_expr(Expr* expr);

static inline
int type_check_bin_expr(Expr* expr) {
	BinaryExpr* bin = expr->content.as_binary;
	if (!type_check_expr(bin->left) || !type_check_expr(bin->right))
		return 0;

	if (bin->left->actual_type.type != &INTEGER)
		return 0;
	if (bin->right->actual_type.type != &INTEGER)
		return 0;

	expr->actual_type.type = &INTEGER;
	expr->actual_type.lvalue = 0;
	expr->actual_type.constant = 0;

	return 1;
}

static inline int type_check_id_expr(Expr* expr) {
	IdExpr* id = expr->content.as_id;
	ActualType *actual_type;

	switch (id->bind->type) {
	case BIND_FPARAM:
		actual_type = &id->bind->content.as_fparam->actual_type;
		break; 
	case BIND_VAR:
		actual_type = &id->bind->content.as_var->actual_type;
		break;
	case BIND_PROC:
		actual_type = &id->bind->content.as_proc->actual_type;
		break;
	case BIND_TYPE:
		return 0;
	default:
		assert(0);
		return 0;
	}

	memcpy(&expr->actual_type, actual_type, sizeof(ActualType));
	return 1;
}

static inline int type_check_num_expr(Expr* expr) {
	expr->actual_type.type = &INTEGER;
	expr->actual_type.lvalue = 0;
	expr->actual_type.constant = 0;
	return 1;
}

static inline
int type_check_call_expr(Expr* expr) {
	int i;
	ProcType* proc_type;
	Param* param;
	CallExpr* call = expr->content.as_call;

	if (!type_check_expr(call->lvalue))
		return 0;

	if (call->lvalue->actual_type.type->kind != TYPE_PROC)
		return 0;
	proc_type = call->lvalue->actual_type.type->content.as_proc;

	param = call->first_param;
	for (i=0; i<proc_type->fparams_count; i++) {
		Type* fp_type;
		Type* value_type;

		fp_type = proc_type->fparams[i];
		if (!param || !type_check_expr(param->value))
			return 0;
		value_type = param->value->actual_type.type;

		if (memcmp(fp_type, value_type, sizeof(Type)) != 0)
			return 0;

		param = param->next;
	}
	if (param)
		return 0;

	expr->actual_type.type = proc_type->return_type;
	expr->actual_type.lvalue = 0;
	expr->actual_type.constant = 0;

	return 1;
}

int type_check_expr(Expr* expr) {
	switch (expr->type) {
	case EXPR_BINARY:
		return type_check_bin_expr(expr);
	case EXPR_ID:
		return type_check_id_expr(expr);
	case EXPR_NUM:
		return type_check_num_expr(expr);
	case EXPR_CALL:
		return type_check_call_expr(expr);
	default:
		assert(0);
	}
	return 0;
}

static int type_check_stmt(Stmt* stmt);

static inline int type_check_assign(AssignStmt* assign) {
	if (!type_check_expr(assign->lvalue) 
		|| !assign->lvalue->actual_type.lvalue
		|| assign->lvalue->actual_type.constant
		|| !type_check_expr(assign->rvalue))
		return 0;

	return (memcmp(assign->lvalue->actual_type.type, 
		assign->rvalue->actual_type.type, sizeof(Type)) == 0);
}

static inline int type_check_for(ForStmt* forstmt) {
	return (type_check_expr(forstmt->from) 
		&& type_check_expr(forstmt->to)
		&& (forstmt->bind->type == BIND_VAR)
		&& (forstmt->bind->content.as_var->actual_type.type == &INTEGER));
}

static inline int type_check_return(ReturnStmt* ret) {
	return (ret->expr && !type_check_expr(ret->expr)) ? 0 : 1;
}

static inline int type_check_call(CallStmt* call) {
	return type_check_expr(call->expr);
}

int type_check_stmt(Stmt* stmt) {
	switch (stmt->type) {
	case STMT_ASSIGN:
		return type_check_assign(stmt->content.as_assign);
	case STMT_FOR:
		return type_check_for(stmt->content.as_for);
	case STMT_RETURN:
		return type_check_return(stmt->content.as_return);
	case STMT_CALL:
		return type_check_call(stmt->content.as_call);
	default:
		assert(0);
		return 0;
	}
}

static inline int type_check_stmts(Stmt* stmt) {
	int ok = 1;
	while (ok && stmt) {
		if (type_check_stmt(stmt))
			stmt = stmt->next;
		else
			ok = 0;
	}
	return ok;
}

int type_check(Prog* prog) {
	int ok = attach_types(prog);
	if (ok) {
		Proc* proc = prog->first_proc;
		while (ok && proc) {
			if (type_check_stmts(proc->first_stmt))
				proc = proc->next;
			else
				ok = 0;
		}
	}
	return ok;
}

void destroy_type(Type* type) {
	assert(type);

	switch (type->kind) {
	case TYPE_UNINITIALIZED:
	case TYPE_INTEGER:
		return;
	case TYPE_PROC:
		return free_proc_type(type->content.as_proc);
	}
}