.global main

.section .data

my_double: .double 64.125

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
	push %rbp
	movq %rsp, %rbp

	# Get pid
	movq $39, %rax
	syscall
	movq %rax, %r12

	# Store to r13
	movq $0xcafecafe, %r13
	trap

	# Store to r13b
	movb $42, %r13b
	trap

	# Store to mm0
	# In case of mmx registers, we can't move an 64bit immediate so we first move
	# it to r13.
	movq $0xba5eba11, %r13
	movq %r13, %mm0
	trap

	# Store to xmm0
	movsd my_double(%rip), %xmm0
	trap

	# Store to st0
	# This means "Empty MMX technology state", it clears the MMX state. This is
	# because MMX and x87 registers share registers.
	emms
	fldl my_double(%rip)
	trap
	
	popq %rbp
	movq $0, %rax
	ret
