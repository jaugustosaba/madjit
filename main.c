#include <stdio.h>
#include <stdlib.h>
#include "parser.h"

int main(int argc, char *argv[]) {
	FILE *fp;
	Prog *prog;
	ParseStatus status;

	fp = fopen("input.txt", "r");
	if (!fp) {
		perror("Cannot open input file");
		return EXIT_FAILURE;
	}

	status = parse((GetChar)fgetc, fp, &prog);
	switch (status) {
		case PARSE_OK:
			printf("Syntax Ok\n");
			if (resolve_binds(prog)) {
				printf("Names Ok\n");
				if (type_check(prog)) {
					printf("Types Ok\n");
					if (compile(prog)) {
						int result;
						printf("Compiling Ok\n");
						dump_interp_prog(&prog->interp, stdout);
						if (eval_interp_prog(&prog->interp, &result))
							printf("Eval %d\n", result);
						if (eval_jit(&prog->interp, &result))
							printf("JIT Eval %d\n", result);
					}
				}
			}
			free_prog(prog);
			break;
		case PARSE_NO_MEM:
			printf("No Mem\n");
			break;
		case PARSE_SYNTAX_ERROR:
			printf("Syntax Error\n");
			break;
	}

	fclose(fp);

	return EXIT_SUCCESS;
}