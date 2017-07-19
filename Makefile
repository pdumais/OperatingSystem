all: image
DISKSIZE=10240 #5 meg divided by 512


.PHONY: image

image:
	cd kernel && make
	cd bootloader && make
	cd tests && make
	cd userapps && make
	mkdir -p image
	dd if=/dev/zero of=image/disk.img count=$(DISKSIZE)
	dd conv=notrunc if=bootloader/bootloader.o of=image/disk.img
	dd conv=notrunc if=kernel/kernel.o of=image/disk.img seek=3

.PHONY: net
net:
	/usr/sbin/tunctl -t tap100
	/usr/sbin/tunctl -t tap200
	/sbin/ifconfig tap100 0.0.0.0 promisc up
	/sbin/ifconfig tap200 0.0.0.0 promisc up
	-/usr/local/bin/ovs-vsctl del-port br0 tap100 > /dev/null 2>&1
	/usr/local/bin/ovs-vsctl add-port br0 tap100
run: net
	qemu-system-x86_64 --enable-kvm -cpu host -smp 4 -option-rom sgabios.bin \
		-m 8192 -rtc base=localtime \
		-monitor stdio -curses \
		-drive file=image/disk.img,if=ide \
		-drive file=userapps/disk.img,if=virtio \
		-drive file=hddtest1/test2.img,if=virtio \
		-net nic,model=virtio,macaddr=52:54:00:12:34:60 -net tap,ifname=tap100,script=no \
		-d cpu_reset -D qemu.log 
#		-net nic,model=rtl8139,macaddr=52:54:00:12:34:61 -net tap,vlan=1,ifname=tap1,script=no \

test: net
	qemu-system-x86_64 --enable-kvm -cpu host -smp 4 -option-rom sgabios.bin \
		-m 8192 -rtc base=localtime \
		-monitor telnet:127.0.0.1:2048,server,nowait,ipv4 -curses \
		-drive file=image/disk.img,if=ide \
		-drive file=userapps/disk.img,if=virtio \
		-drive file=hddtest1/test2.img,if=virtio \
		-net nic,model=virtio,macaddr=52:54:00:12:34:60 -net tap,ifname=tap100,script=no \
		-d cpu_reset -D qemu.log 
#		-net nic,model=rtl8139,macaddr=52:54:00:12:34:61 -net tap,vlan=1,ifname=tap1,script=no \

server: net
	qemu-system-x86_64 --enable-kvm -cpu host -smp 4 -option-rom sgabios.bin \
		-m 4096 -rtc base=localtime \
		--daemonize \
		-drive file=image/disk.img,if=ide \
		-drive file=userapps/disk.img,if=virtio \
		-drive file=hddtest1/test2.img,if=virtio \
		-net nic,model=virtio,macaddr=52:54:00:12:34:60 -net tap,ifname=tap100,script=no \
		-d cpu_reset -D qemu.log 
#		-net nic,model=rtl8139,macaddr=52:54:00:12:34:61 -net tap,vlan=1,ifname=tap1,script=no \
	

#test:
	#qemu-system-x86_64 --enable-kvm -cpu host -smp 4 -monitor telnet:127.0.0.1:3014,server,nowait,ipv4 -option-rom sgabios.bin -hda image/disk.img -hdb userapps/disk.img -curses -net nic,model=rtl8139,macaddr=52:54:00:12:34:60 -net tap,ifname=tap100,script=no -net nic,model=rtl8139,macaddr=52:54:00:12:34:61 -net tap,ifname=tap200,script=no -m 4096 -d int,cpu_reset -D qemu.log -rtc base=localtime 

#	qemu-system-x86_64 --enable-kvm -cpu host -option-rom sgabios.bin -hda image/disk.img -monitor telnet:127.0.0.1:2048,server,nowait,ipv4 -nographic -no-reboot -net nic,model=rtl8139,macaddr=52:54:00:12:34:60 -net tap4,vlan=0,ifname=tap2,script=no  -net nic,model=rtl8139,macaddr=52:54:00:12:34:61 -net tap,vlan=1,ifname=tap3,script=no -D qemu.log -d int,in_asm

clean:
	cd kernel && make clean
	cd userapps && make clean

disasm:
	#objdump -D -b binary -mi386 -M x86-64 kernel/kernel.o 
	objdump -D -mi386 -M x86-64 kernel/kerneldump.o 

tunctl:
	tunctl -u pat -t tap2 
	tunctl -u pat -t tap4 
	chown root:sysadmin /dev/kvm
	chmod 660 /dev/kvm
