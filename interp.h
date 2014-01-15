#ifndef INTERP_H
#define INTERP_H

#include <stdio.h>

typedef enum InterpOp {
	INTERP_INVALID = 0,
	INTERP_PUSH,
	INTERP_POP,
	INTERP_LOAD,
	INTERP_STORE,
	INTERP_VAR,
	INTERP_PARAM,
	INTERP_PROC,
	INTERP_DUP,
	INTERP_ADD,
	INTERP_MUL,
	INTERP_INC,
	INTERP_CMP,
	INTERP_JMP,
	INTERP_JLT,
	INTERP_CALL,
	INTERP_CALLV,
	INTERP_RETV,
	INTERP_RET,
} InterpOp;

typedef struct InterpInstr {
	InterpOp  op;
	int       value;
} InterpInstr;

typedef struct InterpCode {
	int           size;
	InterpInstr*  data;
} InterpCode;

typedef struct InterpProg {
	int          proc_count;
	int          main;
	InterpCode*  procs;
} InterpProg;

int init_interp_code(InterpCode* code, int size);

int init_interp(InterpProg* interp_prog, int proc_count);

int dump_interp_code(InterpCode* code, FILE* fp);

int dump_interp_prog(InterpProg* prog, FILE* fp);

void destroy_interp(InterpProg* interp_prog);

void destroy_interp_code(InterpCode* code);

#endif