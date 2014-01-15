#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "interp.h"

typedef struct Type     Type;
typedef struct FParam   FParam;
typedef struct Bind     Bind;
typedef struct Context  Context;
typedef struct Var      Var;
typedef struct Expr     Expr;
typedef struct Param    Param;
typedef struct Stmt     Stmt;
typedef struct Proc     Proc;

typedef enum TypeKind {
	TYPE_UNINITIALIZED = 0,
	TYPE_INTEGER,
	TYPE_PROC,
} TypeKind;

typedef struct ProcType {
	int     fparams_count;
	Type**  fparams;
	Type*   return_type;
} ProcType;

struct Type {
	TypeKind kind;
	union {
		ProcType*  as_proc;
	} content;
};

typedef struct ActualType {
	Type*  type;
	int    lvalue;
	int    constant;
} ActualType;

struct FParam {
	int          nid;
	char         id[MAX_LEXEME_SIZE+1];
	char         type[MAX_LEXEME_SIZE+1];
	Bind*        type_bind;
	ActualType   actual_type;
	FParam*      next; 
};

struct Var {
	int          nid;
	char         id[MAX_LEXEME_SIZE+1];
	char         type[MAX_LEXEME_SIZE+1];
	Bind*        type_bind;
	ActualType   actual_type;
	Var*         next;
};

typedef enum ExprType {
	EXPR_BINARY,
	EXPR_ID,
	EXPR_NUM,
	EXPR_CALL
} ExprType;

typedef enum BinaryOp {
	OP_ADD,
	OP_MULT
} BinaryOp;

typedef struct BinaryExpr {
	BinaryOp  op;
	Expr*     left;
	Expr*     right;	
} BinaryExpr;

typedef struct IdExpr {
	char   name[MAX_LEXEME_SIZE+1];
	Bind*  bind;
} IdExpr;

typedef struct NumExpr {
	int value;
} NumExpr;

struct Param {
	Expr*   value;
	Param*  next;
};

typedef struct CallExpr {
	Expr*   lvalue;
	Param*  first_param;
} CallExpr;

struct Expr {
	ExprType         type;
	union {
		BinaryExpr*  as_binary;
		IdExpr*      as_id;
		NumExpr*     as_num;
		CallExpr*    as_call;
	} content;
	ActualType       actual_type;
};

typedef enum StmtType {
	STMT_ASSIGN,
	STMT_FOR,
	STMT_RETURN,
	STMT_CALL
} StmtType;

typedef struct AssignStmt {
	Expr*   lvalue;
	Expr*   rvalue;
} AssignStmt;

typedef struct ForStmt {
	char    id[MAX_LEXEME_SIZE+1];
	Bind*   bind;
	Expr*   from;
	Expr*   to;
	Stmt*   first_stmt;
} ForStmt;

typedef struct ReturnStmt {
	Proc*  proc;
	Expr*  expr;
} ReturnStmt;

typedef struct CallStmt {
	Expr* expr;
} CallStmt;

struct Stmt {
	StmtType         type;
	union {
		AssignStmt*  as_assign;
		ForStmt*     as_for;
		ReturnStmt*  as_return;
		CallStmt*    as_call;
	} content;
	Stmt*            next;
};

typedef enum BindType {
	BIND_FPARAM,
	BIND_VAR,
	BIND_PROC,
	BIND_TYPE
} BindType;

struct Bind {
	const char*   id;
	BindType      type;
	union {
		FParam*   as_fparam;
		Var*      as_var;
		Proc*     as_proc;
		Type*     as_type;
	} content;
	Bind*         next;
};

struct Context {
	Context*  upper_context;
	Bind*     first_bind;
};

struct Proc {
	int         nid;
	Context     ctx;
	Type        type;
	ActualType  actual_type;
	char        id[MAX_LEXEME_SIZE+1];
	int         fparam_count;
	FParam*     first_fparam;
	int         is_function;
	char        return_type[MAX_LEXEME_SIZE+1];
	Bind*       return_type_bind;
	ActualType  actual_return_type;
	int         var_count;
	Var*        first_var;
	Stmt*       first_stmt;
	int         mismatch;
	Proc*       next;
};

typedef struct Prog {
	Context     ctx;
	int         proc_count;
	InterpProg  interp;
	Proc*       first_proc;
} Prog;

typedef enum ParseStatus {
	PARSE_OK,
	PARSE_NO_MEM,
	PARSE_SYNTAX_ERROR
} ParseStatus;

/* context */

void init_context(Context* context, Context* upper_context);

void free_context(Context* context);

Bind* local_lookup(Context* context, const char* id);

Bind* lookup(Context* context, const char* id);

Type* lookup_type(Context* context, const char* id);

Bind* bind_fparam(Context* context, const char* id, FParam* fparam);

Bind* bind_var(Context* context, const char* id, Var* var);

Bind* bind_proc(Context* context, const char* id, Proc* proc);

Bind* bind_type(Context* context, const char* id, Type* type);

/* parsing */

ParseStatus parse(GetChar input_fun, void* user_data, Prog **result);

void free_prog(Prog *prog);

/* name resolution */
int resolve_binds(Prog* prog);

/* type checker */
Type INTEGER;

int type_check(Prog* prog);

void destroy_type(Type* type);

/* code generator */

int compile(Prog* prog);


#endif