  .text
  .global syscall
  .align 8
  .type syscall,@function

syscall:
  mov    %rdi,%rax
  mov    %rsi,%rdi
  mov    %rdx,%rsi
  mov    %rcx,%rdx
  mov    %r8,%r10
  mov    %r9,%r8
  mov    0x8(%rsp),%r9
  syscall
  retq
