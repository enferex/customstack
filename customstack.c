#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASM(_expr) asm volatile(_expr)

static void print_stack_bounds(void)
{
    void *bp, *sp;

    asm volatile("mov %%rdi, %0\n"
                 "mov %%rsi, %1\n" : "=r"(bp), "=r"(sp));

    printf("Stack Base: %p\n"
           "Stack Top:  %p\n",
           bp, sp);
}

#define DUMP(_msg)             \
   do {                        \
       printf(_msg "\n");      \
       ASM("mov %rbp, %rdi\n"  \
           "mov %rsp, %rsi\n");\
       print_stack_bounds();   \
   } while (0)

static int example(void)
{
    int a, b, c, result, stack_size;
    void *new_sp;

    // Initial values in this stack frame.  The values will be copied to the
    // dynamic 'cloned' stack.
    a = 1;
    b = 2;
    c = 3;
    result = 42;
    
    DUMP("Before custom stack...");

    // Save the existing stack and '0' which we will replace with the stack size
    // We need to pre-allocate now since we want to copy this to the new
    // stack so we can restore back later.
    ASM("push %rsp\n"
        "push %rbp\n"
        "push $0\n"  // Place holdser
        "push $0\n");

    // Stack looks like:  Top --> [0, 0, rbp, rsp, ...]

    // Calculate stack size (rbp - rsp).
    // Assumes stack grows downward towards 0, so the rbp
    // should have the higher address and rsp the lower.
    // NOTE: Keep the stack size constant through out now:  No more pushes
    // without pops.
    ASM("mov  %rsp, %rcx\n" // rcx <- rsp
        "mov  %rbp, %rax\n" // rax <- rbp
        "sub  %rcx, %rax\n" // rax <- rcx - rax
        "pop  %rdx\n"       // Trash the placeholder '0'
        "push %rax\n");     // Size is at the top
    
    // Stack looks like:  Top --> [stack_size, 0, rbp, rsp, ...]

    // Allocate a new stack of the correct size.
    ASM("mov $12, %rax\n"  // rax <- syscall number for brk
        "mov $0, %rdi\n"   // arg0
        "syscall\n"        // rax <- syscall(brk, 0);
        "pop %rdi\n"       // rdi <- stack_size
        "push %rdi\n"      // Put the stack size back (need it later)
        "add %rax, %rdi\n" // rdi <- brk + stack_size
        "mov  $12, %rax\n" // rax <- syscall number for brk
        "syscall\n");      // rax <- new heap addr

    // Stack looks like:  Top --> [stack_size, 0, rbp, rsp, ...]

    // Before we update any pointers, copy the current stack to the
    // area for the new stack.
    ASM("mov %rax, %rdi\n"  // Destination (new stack)
        "pop %rcx\n"        // Number of bytes to copy (stack_size)
        "pop %rdx\n"        // Discard the 0 placeholder
        "sub %rcx, %rdi\n"  // rdi <- rdi - stack_size  this is the new rsp
        "push %rcx\n"       // Want the size later, save it now.
        "push %rdi\n"       // Stack [new_rsp, stack_size, old_rbp, old_rsp, ...]
        "mov  %rsp, %rsi\n" // Set the source for the rep move
        "rep movsb\n");

    // Stack looks like:  Top --> [new_rsp, stack_size, old_rbp, old_rsp, ...]

    // We have cloned the stack, now update the pointers.
    ASM("pop %rsp\n"
        "mov %rdi, %rbp\n");

    // Stack looks like:  Top --> [new_rsp, stack_size, old_rbp, old_rsp, ...]

    // Now we are in our cloned stack! *dance*
    DUMP("Using custom stack...");

    // Now we are rocking the new stack.
    printf("a=%d, b=%d, c=%d\n", a, b, c);

    // Do stuff...
    result = a + b + c; // 1 + 2 + 3 --> 6
    printf("Result=%d\n", result);

    // Pluck the values we need 
    ASM("pop %rdx\n"   // Discard new_rsp
        "pop %rdx\n"   // Discard stack_size
        "pop %r14\n"   // r14 <- original rbp
        "pop %r15\n"); // r15 <- original rsp

    // Reclaim 
    ASM("mov $12,  %rax\n"
        "mov %rbp, %rdi\n"
        "syscall\n");

    // Restore original stack pointers
    ASM("mov %r14, %rbp\n"
        "mov %r15, %rsp\n");
 
    DUMP("Using the original stack...");

    /* Since we do not copy the modified stack back, 'result' should be 42. */
    return result;
}

int main(void)
{
    int result = example();
    printf("Result: %d\n", result); // Should be 42
    return 0;
}
