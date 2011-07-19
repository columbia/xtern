	.file	"external-call-test.ll"
	.text
	.globl	main
	.align	16, 0x90
	.type	main,@function
main:                                   # @main
# BB#0:                                 # %entry
	subq	$24, %rsp
	leaq	8(%rsp), %rdi
	xorl	%esi, %esi
	callq	gettimeofday
	addq	$24, %rsp
	ret
	.size	main, .-main


	.section	.note.GNU-stack,"",@progbits
