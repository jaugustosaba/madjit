#include "jit.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

typedef unsigned char uchar;

/*
INTERP_PUSH:
  movq ..., %rax
  pushq %rax
*/
typedef uchar PushCode[11];

static const PushCode PUSH = {
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // movq ..., %rax
	0x50 // pushq %rax
};

/*
INTERP_POP:
	popq %rax
*/

typedef uchar PopCode[1];

static const PopCode POP = {
	0x58 // pop %rax
};

/*
INTERP_LOAD:
  popq %rax
  movq (%rax), %rax
  pushq %rax
*/

typedef uchar LoadCode[5];

static const LoadCode LOAD[5] = {
	0x58, // pop %rax
	0x48, 0x8B, 0x00, // mov (%rax), %rax
	0x50 // pushq %rax
};

/*
INTERP_STORE:
  popq %rax
  popq %rcx
  movq %rax, (%rcx)
*/

typedef uchar StoreCode[5];

static const StoreCode STORE[5] = {
	0x59, // pop %rcx
	0x58, // pop %rax
	0x48, 0x89, 0b00000001 // movq %rax, (%rcx)
};

/*
INTERP_VAR:
  movq %rbp, %rax
  addq ..., %rax
*/

typedef uchar VarCode[8];

static const VarCode VAR = {
	0x48, 0x89, 0xe8, // movq %rbp, %rax
	0x48, 0x83, 0xe8, 0x00, // subq ..., %rax
	0x50 // pushq %rax
};

typedef uchar ParamCode[8];

static const ParamCode PARAM = {
	0x48, 0x89, 0xe8, // movq %rbp, %rax
	0x48, 0x83, 0xc0, 0x00, // addq ..., %rax
	0x50 // pushq %rax
};

/*
INTERP_PROC:
  movq ..., %rax
  pushq %rax
*/

typedef uchar ProcCode[11];

static const ProcCode PROC = {
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // movq ..., %rax
	0x50 // pushq %rax
};

/*
INTERP_DUP:
  movq (%rsp), %rax
  pushq %rax
*/

typedef uchar DupCode[5];

static const DupCode DUP = {
	0x48, 0x8b, 0x04, 0x24, // movq (%rsp), %rax
	0x50 // pushq %rax
};

/*
INTERP_ADD:
  popq %rcx
  popq %rax
  addq %rcx, %rax
  pushq %rax
*/

typedef uchar AddCode[6];

static const AddCode ADD = {
	0x59, // pop %rcx
	0x58, // pop %rax
	0x48, 0x03, 0b11000001, // addq %rcx, %rax
	0x50 // pushq %rax
};

/*
INTERP_MUL:
  popq %rcx
  popq %rax
  imulq %rcx, %rax
  pushq %rax
*/

typedef uchar MulCode[7];

static const MulCode MUL = {
	0x59, // pop %rcx
	0x58, // pop %rax
	0x48, 0x0F, 0xAF, 0b11000001, // imul %rcx, %rax
	0x50 // pushq %rax
};

/*
INTERP_INC:
  popq %rax
  incq (%rax)
*/

typedef uchar IncCode[4];

static const IncCode INC = {
	0x58, // pop %rax
	0x48, 0xff, 0x00 // incq (%rax)
};

/*
INTERP_CMP:
  popq %rcx
  popq %rax
  subq %rcx, %rax

*/

typedef uchar CmpCode[6];

static const CmpCode CMP = {
	0x59, // pop %rcx
	0x58, // pop %rax
 	0x48, 0x29, 0b11001000, // subq %rcx, %rax
 	0x50 // pushq %rax
};

/*
INTERP_JMP:
  jmpq ...
*/

typedef uchar JmpCode[5];

static const JmpCode JMP = {
	0xE9, 0x00, 0x00, 0x00, 0x00 // jmpq ...
};

/*
INTERP_JLT: 
  popq %rax
  cmpq 0, %rax
  jleq ...
*/

typedef uchar JltCode[12];

static const JltCode JLT = {
	0x58, // pop %rax
	0x3D, 0x00, 0x00, 0x00, 0x00, // cmpq $0, %rax
	0x0f, 0x8c, 0x00, 0x00, 0x00, 0x00 // jleq ...
};

/*
INTERP_CALL:
  popq   %rax
  callq  *%rax
  pushq  %rax
*/

typedef uchar CallCode[4];

static const CallCode CALL = {
	0x58, // pop %rax,
	0xFF, 0xD0, // callq *%rax
	0x50 // pushq %rax
};

/*
INTERP_CALLV:
  popq %rax
  callq  *%rax
*/

typedef uchar CallvCode[3];

static const CallvCode CALLV = {
	0x58, // pop %rax,
	0xFF, 0xD0 // callq *%rax
};

/*
INTERP_RET:
  popq %rax
  mov %rbp, %rsp
  popq %rbp
  retq ...
*/

typedef uchar RetCode[8];

static const RetCode RET = {
	0x58, // popq %rax,
	0x48, 0x89, 0xec, // mov %rbp, %rsp
	0x5d, // popq %rbp
	0xc2, 0x00, 0x00 // retq ...
};

/*
INTERP_RETV:
  mov %rbp, %rsp
  pop %rbp
  retq ...
 */

typedef uchar RetvCode[7];

static const RetCode RETV = {
	0x48, 0x89, 0xec, // mov %rbp, %rsp
	0x5d, // popq %rbp
	0xc2, 0x00, 0x00 // retq ...
};


typedef uchar PrologueCode[4];

// mov %rsp, %rbp
// call ...
// ret
static const PrologueCode PROLOGUE = {
	0x55, // push %rbp
	0x48, 0x89, 0xe5 // mov %rsp, %rbp
};

typedef struct JITInstr {
	InterpOp        op;
	size_t          code_size;
	size_t          rel_offset;
	size_t          abs_offset;
	union {
		PushCode    as_push;
		PopCode     as_pop;
		LoadCode    as_load;
		StoreCode   as_store;
		VarCode     as_var;
		ParamCode   as_param;
		ProcCode    as_proc;
		DupCode     as_dup;
		AddCode     as_add;
		MulCode     as_mul;
		IncCode     as_inc;
		CmpCode     as_cmp;
		JmpCode     as_jmp;
		JltCode     as_jlt;
		CallCode    as_call;
		CallvCode   as_callv;
		RetCode     as_ret;
		RetCode     as_retv;
	} content;
} JITInstr;

typedef struct JITProc {
	size_t     code_size;
	size_t     abs_offset;
	int        instr_count;
	JITInstr*  instrs;
} JITProc;

typedef int (*JITFunction)();

typedef struct JITContext {
	int          proc_count;
	JITProc*     procs;
	size_t       code_size;
	int          main;
} JITContext;

static inline int relative_disp(JITInstr* from, JITInstr* to) {
	long pc = from->rel_offset + from->code_size;
	return (pc > to->rel_offset) ? -(pc - to->rel_offset) : to->rel_offset - pc;
}

static int compile_code(JITProc* jit_proc, InterpCode* code) {
	int i, ok;
	size_t rel_offset;
	JITInstr* instrs = jit_proc->instrs;

	ok = 1;
	rel_offset = sizeof(PrologueCode);
	for (i=0; ok && (i<code->size); i++) {
		assert(i < jit_proc->instr_count);

		InterpInstr* interp_instr = &code->data[i];
		JITInstr* jit_instr = &instrs[i];

		jit_instr->op = interp_instr->op;
		jit_instr->rel_offset = rel_offset;

		switch (interp_instr->op) {
		case INTERP_INVALID:
			ok = 0;
			break;
		case INTERP_PUSH: {
			memcpy(&jit_instr->content.as_push, PUSH, sizeof(PushCode));
			*((long*)&jit_instr->content.as_push[2]) = interp_instr->value;
			jit_instr->code_size = sizeof(PushCode);
			break;
		}
		case INTERP_POP:
			memcpy(&jit_instr->content.as_pop, POP, sizeof(PopCode));
			jit_instr->code_size = sizeof(PopCode);
			break;
		case INTERP_LOAD:
			memcpy(&jit_instr->content.as_load, LOAD, sizeof(LoadCode));
			jit_instr->code_size = sizeof(LoadCode);
			break;
		case INTERP_STORE:
			memcpy(&jit_instr->content.as_store, STORE, sizeof(StoreCode));
			jit_instr->code_size = sizeof(StoreCode);
			break;
		case INTERP_VAR:
			memcpy(&jit_instr->content.as_var, VAR, sizeof(VarCode));
			jit_instr->content.as_var[6] = (uchar) + (8 * interp_instr->value + 8);
			jit_instr->code_size = sizeof(VarCode);
			break;
		case INTERP_PARAM:
			memcpy(&jit_instr->content.as_param, PARAM, sizeof(ParamCode));
			jit_instr->content.as_param[6] = (uchar) + (8 * interp_instr->value + 16);
			jit_instr->code_size = sizeof(ParamCode);
			break;
		case INTERP_PROC:
			memcpy(&jit_instr->content.as_proc, PROC, sizeof(ProcCode));
			*((long*)&jit_instr->content.as_proc[2]) = interp_instr->value;
			jit_instr->code_size = sizeof(ProcCode);
			break;
		case INTERP_DUP:
			memcpy(&jit_instr->content.as_dup, DUP, sizeof(DupCode));
			jit_instr->code_size = sizeof(DupCode);
			break;
		case INTERP_ADD:
			memcpy(&jit_instr->content.as_add, ADD, sizeof(AddCode));
			jit_instr->code_size = sizeof(AddCode);
			break;
		case INTERP_MUL:
			memcpy(&jit_instr->content.as_mul, MUL, sizeof(MulCode));
			jit_instr->code_size = sizeof(MulCode);
			break;
		case INTERP_INC:
			memcpy(&jit_instr->content.as_inc, INC, sizeof(IncCode));
			jit_instr->code_size = sizeof(IncCode);
			break;
		case INTERP_CMP:
			memcpy(&jit_instr->content.as_cmp, CMP, sizeof(CmpCode));
			jit_instr->code_size = sizeof(CmpCode);
			break;
		case INTERP_JMP:
			memcpy(&jit_instr->content.as_jmp, JMP, sizeof(JmpCode));
			jit_instr->code_size = sizeof(JmpCode);
			break;
		case INTERP_JLT:
			memcpy(&jit_instr->content.as_jlt, JLT, sizeof(JltCode));
			jit_instr->code_size = sizeof(JltCode);
			break;
		case INTERP_CALL:
			memcpy(&jit_instr->content.as_call, CALL, sizeof(CallCode));
			jit_instr->code_size = sizeof(CallCode);
			break;
		case INTERP_CALLV:
			memcpy(&jit_instr->content.as_callv, CALLV, sizeof(CallvCode));
			jit_instr->code_size = sizeof(CallvCode);
			break;
		case INTERP_RET:
			memcpy(&jit_instr->content.as_ret, RET, sizeof(RetCode));
			*((short*)&jit_instr->content.as_ret[6]) = (short) 8 * interp_instr->value;
			jit_instr->code_size = sizeof(RetCode);
			break;
		case INTERP_RETV:
			memcpy(&jit_instr->content.as_retv, RETV, sizeof(RetvCode));
			*((short*)&jit_instr->content.as_ret[5]) = (short) 8 * interp_instr->value;
			jit_instr->code_size = sizeof(RetvCode);
			break;
		default:
			assert(0);
			ok = 0;
		}
		rel_offset += jit_instr->code_size;
	}

	jit_proc->code_size = rel_offset;

	for (i=0; ok && (i<code->size); i++) {
		JITInstr* jit_instr = &instrs[i];

		switch (jit_instr->op) {
		case INTERP_JMP: {
			JITInstr* target = &instrs[code->data[i].value];
			(*(int*)&jit_instr->content.as_jmp[1]) = relative_disp(jit_instr, target);
			break;
		}
		case INTERP_JLT: {
			JITInstr* target = &instrs[code->data[i].value];
			(*(int*)&jit_instr->content.as_jlt[8]) = relative_disp(jit_instr, target);
			break;
		}
		}
	}

	return ok;
}

static inline int init_proc(JITProc* proc, InterpCode* code) {
	proc->instr_count = code->size;
	proc->instrs = (JITInstr*) calloc(proc->instr_count, sizeof(JITInstr));
	return proc->instrs ? 1 : 0;
}

static inline int init_context(JITContext* ctx, InterpProg* interp_prog) {
	int i;
	int ok;

	ctx->code_size = 0;
	ctx->main = interp_prog->main;
	ctx->proc_count = interp_prog->proc_count;
	ctx->procs = (JITProc*) calloc(ctx->proc_count, sizeof(JITProc));
	if (!ctx->procs)
		return 0;

	ok = 1;
	for (i=0; ok && (i < ctx->proc_count); i++)
		ok = init_proc(&ctx->procs[i], &interp_prog->procs[i]);

	return ok;
}

static inline void destroy_proc(JITProc* proc) {
	if (proc)
		free(proc->instrs);
}

static inline void destroy_context(JITContext* ctx) {
	int i;
	for (i=0; i<ctx->proc_count; i++)
		destroy_proc(&ctx->procs[i]);
	free(ctx->procs);
}

static inline int compile(JITContext* ctx, InterpProg* interp_prog) {
	int i;
	int ok = 1;

	for (i=0; ok && (i<ctx->proc_count); i++) {
		JITProc* jit_proc = &ctx->procs[i];
		if (compile_code(jit_proc, &interp_prog->procs[i]))
			ctx->code_size += jit_proc->code_size;
		else 
			ok = 0;
	}

	return ok;
}

static inline void link_proc(JITContext* ctx, JITProc* proc) {
	int i;
	size_t abs_offset = proc->abs_offset;
	for (i=0; i<proc->instr_count; i++) {
		JITInstr* jit_instr = &proc->instrs[i];

		jit_instr->abs_offset = jit_instr->rel_offset + proc->abs_offset;

		if (jit_instr->op == INTERP_PROC) {
			long id = *((long*)&jit_instr->content.as_proc[2]);
			*((size_t*)&jit_instr->content.as_proc[2]) = ctx->procs[id].abs_offset;
		}
	}
}

static inline void link(JITContext* ctx, size_t abs_addrs) {
	int i;
	for (i=0; i<ctx->proc_count; i++) {
		JITProc* proc = &ctx->procs[i];
		proc->abs_offset = abs_addrs;
		abs_addrs += proc->code_size;
	}
	for (i=0; i<ctx->proc_count; i++)
		link_proc(ctx, &ctx->procs[i]);
}

static inline void dump_proc(JITProc* jit_proc) {
	int i;

	memcpy((void*)jit_proc->abs_offset, PROLOGUE, sizeof(PROLOGUE));
	for (i=0; i<jit_proc->instr_count; i++) {
		JITInstr* jit_instr = &jit_proc->instrs[i];
		void* addrs = (void*)jit_instr->abs_offset;
		memcpy(addrs, &jit_instr->content, jit_instr->code_size);
	}
}

static inline void dump_exec(JITContext* ctx) {
	int i;
	for (i=0; i<ctx->proc_count; i++)
		dump_proc(&ctx->procs[i]);
}

static inline int execute(JITContext* ctx, int* result) {
	JITFunction fun;
	void* exec_mem;

	exec_mem = mmap(NULL, ctx->code_size, PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (exec_mem == MAP_FAILED)
		return 0;
	link(ctx, (size_t)exec_mem);
	dump_exec(ctx);

	fun = (JITFunction)ctx->procs[ctx->main].abs_offset;
	*result = (*fun)();

	if (munmap(exec_mem, ctx->code_size) == -1)
		return 0;
	return 1;
}

int eval_jit(InterpProg* interp_prog, int* result) {
	int ok;
	JITContext ctx;

	ok = init_context(&ctx, interp_prog);
	if (ok)
		ok = compile(&ctx, interp_prog);
	
	if (ok)
		ok = execute(&ctx, result);

	destroy_context(&ctx);
	return ok;

}
