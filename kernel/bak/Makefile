OUT=../image/
OBJC=display.o pci.o rtl8139.o netcard.o arp.o arpcache.o ip.o
SOURCEC=$(OBJC:.o=.c)
OBJASM=kernelmain.o interrupts.o pic.o scheduler.o helpers.o mutex.o mmu.o
SOURCEASM=$(OBJASM:.o=.S)
CFLAGS=-fno-zero-initialized-in-bss -Wno-pointer-to-int-cast

.c.o:
	gcc -m64 $(CFLAGS) -c $< -o $@
.S.o:
	gcc -m64 -c $< -o $@

all: kernel

kernel: $(OBJASM) $(OBJC)
	@printf "\r\n========== Linking =============\r\n"
	ld -m elf_x86_64 -static --oformat=binary -T link.lds $(OBJASM) $(OBJC) -o kernel.o
	echo -n ".EQU KERNEL_SIZE, " > ../sizekernel.inc
	wc -c < kernel.o >> ../sizekernel.inc
	
clean:
	rm *.o	

