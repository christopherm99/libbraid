.intel_syntax noprefix
.text

// noreturn ctx_swap(Context *, Context *);
.globl ctx_swap
ctx_swap:
  mov [rsi], rsp;
  mov rsp, [rdi];
  mov [rsi + 0x08], rbp;
  mov [rsi + 0x10], rbx;
  mov [rsi + 0x18], r12;
  mov [rsi + 0x20], r13;
  mov [rsi + 0x28], r14;
  mov [rsi + 0x30], r15;
  mov rbp, [rdi + 0x08];
  mov rbx, [rdi + 0x10];
  mov r12, [rdi + 0x18];
  mov r13, [rdi + 0x20];
  mov r14, [rdi + 0x28];
  mov r15, [rdi + 0x30];
  ret;

