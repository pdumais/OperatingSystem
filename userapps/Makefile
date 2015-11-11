all: apps
APPS=$(filter-out systemlib,$(sort $(patsubst ./%/,%,$(dir $(wildcard ./*/Makefile)))))
BINS=$(addprefix appsbin/,$(APPS))
BINS:=$(addsuffix .bin,$(BINS))

.PHONY: systemlib
systemlib:
	 cd systemlib && make

appsbin/%.bin:%
	 cd $< && make

apps: systemlib $(BINS) 
	 ./mkimage.pl
	 cp index.bin disk.img
	 cat apps.bin >> disk.img

clean:
	 cd systemlib && make clean
	 rm -Rf appsbin
	 rm index.bin
	 rm disk.img
	 rm apps.bin
