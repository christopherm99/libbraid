.intel_syntax noprefix
.text

.globl ctxswap
ctxswap:
  mov [rdi], rsp;
  mov rsp, [rsi];
  mov [rdi + 0x08], rbp;
  mov [rdi + 0x10], rbx;
  mov [rdi + 0x18], r12;
  mov [rdi + 0x20], r13;
  mov [rdi + 0x28], r14;
  mov [rdi + 0x30], r15;
  mov rbp, [rsi + 0x08];
  mov rbx, [rsi + 0x10];
  mov r12, [rsi + 0x18];
  mov r13, [rsi + 0x20];
  mov r14, [rsi + 0x28];
  mov r15, [rsi + 0x30];
  mov rdi, [rsi + 0x38];
  ret;

.section .note.GNU-stack,"",@progbits

