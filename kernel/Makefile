OUT=../image/
OBJC=display.o pci.o rtl8139.o netcard.o arp.o arpcache.o ip.o icmp.o ip_routing.o block_cache.o ata.o test.o userboot.o console.o keyboard.o userprocess.o vfs.o flatfs.o sockets.o memorypool.o hashtable.o tree.o vfat.o virtio.o virtnet.o block_device.o virtblock.o vmx_ept.o
SOURCEC=$(OBJC:.o=.c)
OBJASM=boot.o kernelmain.o interrupts.o scheduler.o helpers.o mutex.o mmu.o hardware.o apic.o intA0.o heap.o cmos.o vmx.o guest.o
SOURCEASM=$(OBJASM:.o=.S)
CFLAGS=-fno-zero-initialized-in-bss -Wno-pointer-to-int-cast -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-3dnow -Wimplicit-function-declaration -Werror=implicit-function-declaration
PFLAGS=-DPCID

.c.o:
	gcc -m64 $(CFLAGS) -c $< -o $@
.S.o:
	gcc -m64 -c $< $(PFLAGS) -o $@

all: kernel

kernel: $(OBJASM) $(OBJC)
	@printf "\r\n========== Linking =============\r\n"
	ld -m elf_x86_64 -static --oformat=binary -T link.lds $(OBJASM) $(OBJC) -o kernel.o
	ld -m elf_x86_64 -static -T link.lds $(OBJASM) $(OBJC) -o kerneldump.o
	echo -n ".EQU KERNEL_SIZE, " > ../sizekernel.inc
	wc -c < kernel.o >> ../sizekernel.inc
	
clean:
	rm *.o	

