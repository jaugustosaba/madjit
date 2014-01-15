#include "parser.h"

#include <stdlib.h>
#include <string.h>

static void free_bind(Bind* bind) {
	if (bind->next)
		free_bind(bind->next);
	free(bind);
}

void init_context(Context* context, Context* upper_context) {
	context->upper_context = upper_context;
	context->first_bind = NULL;
}

void free_context(Context* context) {
	if (context->first_bind)
		free_bind(context->first_bind);
}

Bind* local_lookup(Context* context, const char* id) {
	Bind* b = context->first_bind;
	while (b) {
		if (strcmp(id, b->id) == 0)
			return b;
		b = b->next;
	}
	return NULL;
}

Bind* lookup(Context* context, const char* id) {
	Context* ctx = context;
	while (ctx) {
		Bind* b = local_lookup(ctx, id);
		if (b)
			return b;
		ctx = ctx->upper_context;
	}
	return NULL;
}

Type* lookup_type(Context* context, const char* id) {
	Bind *bind = lookup(context, id);
	if (!bind || (bind->type != BIND_TYPE))
		return NULL;
	return bind->content.as_type;
}

static inline Bind* bind_alloc(Context* context, const char* id, BindType type) {
	Bind* b = (Bind*) calloc(1, sizeof(Bind));
	if (!b)
		return NULL;
	b->id = id;
	b->type = type;
	b->next = context->first_bind;
	context->first_bind = b;
	return b;	
}

Bind* bind_fparam(Context* context, const char* id, FParam* fparam) {
	Bind* b = bind_alloc(context, id, BIND_FPARAM);
	if (b)
		b->content.as_fparam = fparam;
	return b;
}

Bind* bind_var(Context* context, const char* id, Var* var) {
	Bind* b = bind_alloc(context, id, BIND_VAR);
	if (b)
		b->content.as_var = var;
	return b;
}

Bind* bind_proc(Context* context, const char* id, Proc* proc) {
	Bind* b = bind_alloc(context, id, BIND_PROC);
	if (b)
		b->content.as_proc = proc;
	return b;
}

Bind* bind_type(Context* context, const char* id, Type* type) {
	Bind* b = bind_alloc(context, id, BIND_TYPE);
	if (b)
		b->content.as_type = type;
	return b;
}