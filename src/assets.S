// El atlas (512x256, RGBA crudo) va dentro del EBOOT. Alineado a 16: lo exige la GE.
	.section .rodata
	.balign 16
	.global atlas_rgba
atlas_rgba:   // dos paginas de 512x512 seguidas
	.incbin "assets/atlas.rgba"
