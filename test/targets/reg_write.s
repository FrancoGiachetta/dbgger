// Define a global symbol main so that the linker can find it.
.global main

.section .data

// Format specifiers for printf.

hex_format: .asciz "%#x"
float_format: .asciz "%.2f"

.section .text

# Issue the kill syscall with SIGTRAP.
.macro trap
	// kill syscall ID is 62.
	movq $62, %rax
	# Store the first param of the syscall (process id).
	movq %r12, %rdi
	# SIGTRAP ID is 5.
	movq $5, %rsi
	syscall
.endm

main:
	// Put the contents of rbp on top of the stack.
	push %rbp
	movq %rsp, %rbp

	# Get pid. getpid syscall id is 39.
	movq $39, %rax
	syscall
	// Store the syscall's result in r12 for future use.
	movq %rax, %r12

	trap

	# Print contents of rsi.
	leaq hex_format(%rip), %rdi
	movq $0, %rax
	call printf@plt
	movq $0, %rdi
	call fflush@plt

	trap

	# Print contents of mm0. To do so, first move its contents into rsi.
	movq %mm0, %rsi

	leaq hex_format(%rip), %rdi
	movq $0, %rax
	call printf@plt
	movq $0, %rdi
	call fflush@plt

	trap
	
	# Print contents of xmm0 (sse). To do so, first move its contents into rsi.
	leaq float_format(%rip), %rdi
	movq $1, %rax
	call printf@plt
	movq $0, %rdi
	call fflush@plt

	trap

	// Restore the valu of rbp, which is on top of the stack due to the first
	// isnstruction.
	popq %rbp
	// Make main return 0.
	movq $0, %rax
	ret
