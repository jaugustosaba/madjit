.PHONY: clean

main: main.c lexer.h lexer.c parser.h parser.c symbol-table.c binds.c type-checker.c interp.h interp.c code-gen.c jit.h jit.c
	gcc main.c lexer.c parser.c symbol-table.c binds.c type-checker.c interp.c code-gen.c jit.c -o main -g

clean:
	rm main