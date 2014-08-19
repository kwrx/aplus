[BITS 32]


global read_eip
read_eip:
	pop eax
	jmp eax
ret

global write_eip
write_eip:
	mov ecx, [esp + 4]
	jmp ecx
ret


	

	
