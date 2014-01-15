#include "interp.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

int init_interp_code(InterpCode* code, int size) {
	code->size = size;
	code->data = (InterpInstr*) calloc(size, sizeof(InterpInstr));
	return code->data ? 1 : 0;
}

int init_interp(InterpProg* interp_prog, int proc_count) {
	int i;
	interp_prog->proc_count = proc_count;
	interp_prog->procs = (InterpCode*) calloc(proc_count, sizeof(InterpCode));
	return interp_prog->procs ? 1 : 0;
}

static inline int dump_write(FILE* fp, const char* fmt, ...) {
	int ok;
	va_list ap;
	va_start(ap, fmt);
	ok = vfprintf(fp, fmt, ap) > 0;
	va_end(ap);
	return ok;
}

static inline int dump_instr(FILE* fp, InterpInstr* instr) {
	switch (instr->op) {
	case INTERP_INVALID:
		return dump_write(fp, "INVALID");
	case INTERP_PUSH:
		return dump_write(fp, "PUSH %d", instr->value);
	case INTERP_POP:
		return dump_write(fp, "POP");
	case INTERP_LOAD:
		return dump_write(fp, "LOAD");
	case INTERP_STORE:
		return dump_write(fp, "STORE");
	case INTERP_VAR:
		return dump_write(fp, "VAR %d", instr->value);
	case INTERP_PARAM:
		return dump_write(fp, "PARAM %d", instr->value);
	case INTERP_PROC:
		return dump_write(fp, "PROC %d", instr->value);
	case INTERP_DUP:
		return dump_write(fp, "DUP");
	case INTERP_ADD:
		return dump_write(fp, "ADD");
	case INTERP_MUL:
		return dump_write(fp, "MUL");
	case INTERP_INC:
		return dump_write(fp, "INC");
	case INTERP_CMP:
		return dump_write(fp, "CMP");
	case INTERP_JMP:
		return dump_write(fp, "JMP %d", instr->value);
	case INTERP_JLT:
		return dump_write(fp, "JLT %d", instr->value);
	case INTERP_CALL:
		return dump_write(fp, "CALL");
	case INTERP_CALLV:
		return dump_write(fp, "CALLV");
	case INTERP_RETV:
		return dump_write(fp, "RETV %d", instr->value);
	case INTERP_RET:
		return dump_write(fp, "RET %d", instr->value);
	default:
		assert(0);
	}
	return 0;
}

int dump_interp_code(InterpCode* code, FILE* fp) {
	int i;
	int ok = 1;
	for (i=0; ok && (i < code->size); i++)
		ok = dump_write(fp, "%6d: ", i) 
			&& dump_instr(fp, &code->data[i])
			&& dump_write(fp, "\n");
	return ok;
}

int dump_interp_prog(InterpProg* prog, FILE* fp) {
	int i, ok;
	ok = 1;
	for (i=0; ok && (i<prog->proc_count); i++)
		ok = dump_write(fp, "Proc (%d)\n", i) 
			&& dump_interp_code(&prog->procs[i], fp);
	return ok;
}

#define INTERP_STACK   10*1024

typedef struct InterpStack {
	long  data[INTERP_STACK];
	int   sp;
} InterpStack;

static inline void push(InterpStack* stack, long value) {
	stack->data[stack->sp++] = value;
}

static inline long pop(InterpStack* stack) {
	return stack->data[--stack->sp];
}

static inline long peek(InterpStack* stack) {
	return stack->data[stack->sp - 1];
}

static int eval_interp_code(InterpProg* prog, InterpCode* code, InterpStack* stack, int* result) {
	int bp = stack->sp;
	int pc = 0;

	while (1) {
		assert((pc >= 0) && (pc < code->size));

		InterpInstr *instr = &code->data[pc];
		switch (instr->op) {
		case INTERP_INVALID:
			return 0;
		case INTERP_PUSH:
			push(stack, instr->value);
			break;
		case INTERP_POP:
			pop(stack);
			break;
		case INTERP_LOAD: {
			long* addrs = (long*)pop(stack);
			push(stack, *addrs);
			break;
		}
		case INTERP_STORE: {
			long* addrs = (long*) pop(stack);
			long value = pop(stack);
			*addrs = value;
			break;
		}
		case INTERP_VAR: {
			push(stack, (long) &stack->data[bp + instr->value]);
			break;
		}
		case INTERP_PARAM:
			push(stack, (long) &stack->data[bp - instr->value - 1]);
			break;
		case INTERP_PROC:
			push(stack, instr->value);
			break;
		case INTERP_DUP: {
			long value = peek(stack);
			push(stack, value);
			break;
		}
		case INTERP_ADD: {
			int op2 = pop(stack);
			int op1 = pop(stack);
			push(stack, op1 + op2);
			break;
		}
		case INTERP_MUL: {
			int op2 = pop(stack);
			int op1 = pop(stack);
			push(stack, op1 * op2);
			break;
		}	
		case INTERP_INC: {
			long* addrs = (long*)pop(stack);
			(*addrs)++;
			break;
		}
		case INTERP_CMP: {
			int op2 = pop(stack);
			int op1 = pop(stack);
			push(stack, op1 - op2);
			break;
		}
		case INTERP_JMP:
			pc = instr->value;
			continue;
		case INTERP_JLT:
			if (pop(stack) < 0) {
				pc = instr->value;
				continue;
			}
			break;
		case INTERP_CALL: {
			int id = pop(stack);
			int value;
			if (!eval_interp_code(prog, &prog->procs[id], stack, &value))
				return 0;
			push(stack, value);
			break;
		}
		case INTERP_CALLV: {
			int id = pop(stack);
			if (!eval_interp_code(prog, &prog->procs[id], stack, NULL))
				return 0;
			break;
		}
		case INTERP_RETV: {
			int i;
			assert(result == NULL);

			stack->sp = bp;
			for (i=0; i<instr->value; i++)
				pop(stack);			
			return 1;
		}
		case INTERP_RET: {
			int i;

			assert(result != NULL);
			*result = pop(stack);
			
			stack->sp = bp;
			for (i=0; i<instr->value; i++)
				pop(stack);
			return 1;
		}
		default:
			assert(0);
		}
		pc++;
	}
	return 0;
}

int eval_interp_prog(InterpProg* prog, int* result) {
	InterpStack stack = {.sp = 0};
	if (!eval_interp_code(prog, &prog->procs[prog->main], &stack, result))
		return 0;
	return 1;
}

void destroy_interp(InterpProg* prog) {
	int i;
	for (i=0; i<prog->proc_count; i++)
		destroy_interp_code(&prog->procs[i]);
	free(prog->procs);
}

void destroy_interp_code(InterpCode* code) {
	free(code->data);
}