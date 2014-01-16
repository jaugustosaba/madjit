madjit
======

A complete implementation of a simple VM with code generation on the fly for intel x86-64. The input language is Pascal/Ada like.

## System Requirements
* Ubuntu 12.12 (64 bits required)
* gcc >= 4.8
* make >= 3.81
* gdb >= 7.61
* valgrind >= 3.81

## Lessons Leaned

* return int on error codes is fine
* return a status enum as error is cumbersome
* rax, rbx, rcx, ... are the x64 registers for intel
* do not invent a new calling convention
* gdb works well with assembly
* `as` and `objdump -S` for quick opcode generation
* stack code in native x86 is a mess (too big, lots of pushes and pops)
* no workaround will help you to write code fast (neither fast code)
* data structures matter
* pointers matter too

## TODOs

* implement if (yes, if statement is missing)
* user register vm
* implement bool
* return statement only as last statement in procedure

## Files

Main files description.

### main.c
File input and compiler pass manager.

### lexer.c and lexer.h  
Tokenization.

### parser.c and parser.h
Parsing and AST construction.

### symbol-table.c
Name lookup structure.

### binds.c
Resolve names references to other language elements.

### type-checker.c
Symple type checker.

### code-gen.c
Stack code generation.

### interp.c and interp.h
Definition and evaluation of stack code.

### jit.c and jit.h
Native code generation and output.

## Sample

    procedure fat(n : integer) : integer;
      var i, res : integer;
    begin
      res := 1;
      for i := 1 to n do
        res := res * i;
      done;
      return res;
    end fat;
    
    procedure main() : integer;
    begin
      return fat(5);
    end main;

## Stack code (Intermediate Code)

    Proc (0)
         0: PUSH 0
         1: PUSH 0
         2: PUSH 1
         3: VAR 1
         4: STORE
         5: PUSH 1
         6: VAR 0
         7: STORE
         8: PARAM 0
         9: LOAD
        10: DUP
        11: VAR 0
        12: LOAD
        13: CMP
        14: JLT 25
        15: VAR 1
        16: LOAD
        17: VAR 0
        18: LOAD
        19: MUL
        20: VAR 1
        21: STORE
        22: VAR 0
        23: INC
        24: JMP 10
        25: POP
        26: VAR 1
        27: LOAD
        28: RET 1
    Proc (1)
         0: PUSH 5
         1: PROC 0
         2: CALL
         3: RET 0

## Native Code (dump via gdb)

    0x00007ffff7ff5000:	push   %rbp
    0x00007ffff7ff5001:	mov    %rsp,%rbp
    0x00007ffff7ff5004:	movabs $0x0,%rax
    0x00007ffff7ff500e:	push   %rax
    0x00007ffff7ff500f:	movabs $0x0,%rax
    0x00007ffff7ff5019:	push   %rax
    0x00007ffff7ff501a:	movabs $0x1,%rax
    0x00007ffff7ff5024:	push   %rax
    0x00007ffff7ff5025:	mov    %rbp,%rax
    0x00007ffff7ff5028:	sub    $0x10,%rax
    0x00007ffff7ff502c:	push   %rax
    0x00007ffff7ff502d:	pop    %rcx
    0x00007ffff7ff502e:	pop    %rax
    0x00007ffff7ff502f:	mov    %rax,(%rcx)
    0x00007ffff7ff5032:	movabs $0x1,%rax
    0x00007ffff7ff503c:	push   %rax
    0x00007ffff7ff503d:	mov    %rbp,%rax
    0x00007ffff7ff5040:	sub    $0x8,%rax
    0x00007ffff7ff5044:	push   %rax
    0x00007ffff7ff5045:	pop    %rcx
    0x00007ffff7ff5046:	pop    %rax
    0x00007ffff7ff5047:	mov    %rax,(%rcx)
    0x00007ffff7ff504a:	mov    %rbp,%rax
    0x00007ffff7ff504d:	add    $0x10,%rax
    0x00007ffff7ff5051:	push   %rax
    0x00007ffff7ff5052:	pop    %rax
    0x00007ffff7ff5053:	mov    (%rax),%rax
    0x00007ffff7ff5056:	push   %rax
    0x00007ffff7ff5057:	mov    (%rsp),%rax
    0x00007ffff7ff505b:	push   %rax
    0x00007ffff7ff505c:	mov    %rbp,%rax
    0x00007ffff7ff505f:	sub    $0x8,%rax
    0x00007ffff7ff5063:	push   %rax
    0x00007ffff7ff5064:	pop    %rax
    0x00007ffff7ff5065:	mov    (%rax),%rax
    0x00007ffff7ff5068:	push   %rax
    0x00007ffff7ff5069:	pop    %rcx
    0x00007ffff7ff506a:	pop    %rax
    0x00007ffff7ff506b:	sub    %rcx,%rax
    0x00007ffff7ff506e:	push   %rax
    0x00007ffff7ff506f:	pop    %rax
    0x00007ffff7ff5070:	cmp    $0x0,%eax
    0x00007ffff7ff5075:	jl     0x7ffff7ff50ba
    0x00007ffff7ff507b:	mov    %rbp,%rax
    0x00007ffff7ff507e:	sub    $0x10,%rax
    0x00007ffff7ff5082:	push   %rax
    0x00007ffff7ff5083:	pop    %rax
    0x00007ffff7ff5084:	mov    (%rax),%rax
    0x00007ffff7ff5087:	push   %rax
    0x00007ffff7ff5088:	mov    %rbp,%rax
    0x00007ffff7ff508b:	sub    $0x8,%rax
    0x00007ffff7ff508f:	push   %rax
    0x00007ffff7ff5090:	pop    %rax
    0x00007ffff7ff5091:	mov    (%rax),%rax
    0x00007ffff7ff5094:	push   %rax
    0x00007ffff7ff5095:	pop    %rcx
    0x00007ffff7ff5096:	pop    %rax
    0x00007ffff7ff5097:	imul   %rcx,%rax
    0x00007ffff7ff509b:	push   %rax
    0x00007ffff7ff509c:	mov    %rbp,%rax
    0x00007ffff7ff509f:	sub    $0x10,%rax
    0x00007ffff7ff50a3:	push   %rax
    0x00007ffff7ff50a4:	pop    %rcx
    0x00007ffff7ff50a5:	pop    %rax
    0x00007ffff7ff50a6:	mov    %rax,(%rcx)
    0x00007ffff7ff50a9:	mov    %rbp,%rax
    0x00007ffff7ff50ac:	sub    $0x8,%rax
    0x00007ffff7ff50b0:	push   %rax
    0x00007ffff7ff50b1:	pop    %rax
    0x00007ffff7ff50b2:	incq   (%rax)
    0x00007ffff7ff50b5:	jmpq   0x7ffff7ff5057
    0x00007ffff7ff50ba:	pop    %rax
    0x00007ffff7ff50bb:	mov    %rbp,%rax
    0x00007ffff7ff50be:	sub    $0x10,%rax
    0x00007ffff7ff50c2:	push   %rax
    0x00007ffff7ff50c3:	pop    %rax
    0x00007ffff7ff50c4:	mov    (%rax),%rax
    0x00007ffff7ff50c7:	push   %rax
    0x00007ffff7ff50c8:	pop    %rax
    0x00007ffff7ff50c9:	mov    %rbp,%rsp
    0x00007ffff7ff50cc:	pop    %rbp
    0x00007ffff7ff50cd:	retq   $0x8
    0x00007ffff7ff50d0:	push   %rbp
    0x00007ffff7ff50d1:	mov    %rsp,%rbp
    0x00007ffff7ff50d4:	movabs $0x5,%rax
    0x00007ffff7ff50de:	push   %rax
    0x00007ffff7ff50df:	movabs $0x7ffff7ff5000,%rax
    0x00007ffff7ff50e9:	push   %rax
    0x00007ffff7ff50ea:	pop    %rax
    0x00007ffff7ff50eb:	callq  *%rax
    0x00007ffff7ff50ed:	push   %rax
    0x00007ffff7ff50ee:	pop    %rax
    0x00007ffff7ff50ef:	mov    %rbp,%rsp
    0x00007ffff7ff50f2:	pop    %rbp
    0x00007ffff7ff50f3:	retq   $0x0

## Links:

* [X86 GAS Syntax](http://en.wikibooks.org/wiki/X86_Assembly/GAS_Syntax)
* [x86-64 Opcodes](http://ref.x86asm.net/coder64.html)
* [Intel instructions sizes](http://www.swansontec.com/sintel.html)
* [Instruction Encoding](http://wiki.osdev.org/X86-64_Instruction_Encoding)
* [Encoding Real x86 Instructions](http://www.c-jump.com/CIS77/CPU/x86/lecture.html)
* [Show current instruction in GDB](http://stackoverflow.com/questions/1902901/show-current-instruction-in-gdb)
* [Intel developer manual](http://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2a-manual.pdf)
* [x86 calling convetions](http://www.unixwiz.net/techtips/win32-callconv-asm.html)
