all: bootloader

bootloader: bootloader.s
	gcc -c -m32 bootloader.S -o bootloader.o
	objcopy bootloader.o -O binary 

disasm:
	ndisasm bootloader.o
	

