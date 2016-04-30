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
	tar -cf disk.img -C appsbin/ .
	tar -rf disk.img -C bulkfiles/ .

clean:
	 cd systemlib && make clean
	 rm -Rf appsbin
