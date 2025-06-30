.text

.globl _swapctx
_swapctx:
  mov x16, sp;
  stp x16, x30, [x0];
  ldp x16, x30, [x1];
  mov sp, x16;
  stp x19, x20, [x0, #0x10];
  ldp x19, x20, [x1, #0x10];
  stp x21, x22, [x0, #0x20];
  ldp x21, x22, [x1, #0x20];
  stp x23, x24, [x0, #0x30];
  ldp x23, x24, [x1, #0x30];
  stp x25, x26, [x0, #0x40];
  ldp x25, x26, [x1, #0x40];
  stp x27, x28, [x0, #0x50];
  ldp x27, x28, [x1, #0x50];
  str x29, [x0, #0x60];
  ldr x29, [x1, #0x60];
  stp d8, d9, [x0, #0x70];
  ldp d8, d9, [x1, #0x70];
  stp d10, d11, [x0, #0x80];
  ldp d10, d11, [x1, #0x80];
  stp d12, d13, [x0, #0x90];
  ldp d12, d13, [x1, #0x90];
  stp d14, d15, [x0, #0xa0];
  ldp d14, d15, [x1, #0xa0];
  ldr x0, [x1, #0xa8];
  br x30;
