[BITS 32]

global read_eip
read_eip:
	mov eax, [esp]
ret


global forkchild
forkchild:
	mov esp, eax
	xor eax, eax
ret