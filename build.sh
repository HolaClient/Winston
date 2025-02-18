rm -f linux.o linux

nasm -f elf32 linux.asm -o linux.o && \
ld -m elf_i386 linux.o -o linux && \
./linux
