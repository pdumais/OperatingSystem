TARGET=hexedit
SOURCES=main.c
BINDIR=../appsbin/
OBJECTS=$(SOURCES:%.c=%.o)
SYSTEMLIB=../systemlib
CFLAGS=-I $(SYSTEMLIB)/ -fno-exceptions -I../../kernel/includes

all: $(TARGET).bin

$(OBJECTS): %.o : %.c
	mkdir -p $(BINDIR)
	gcc -ffreestanding $(CFLAGS) -c $< -o $@


$(TARGET).bin: $(OBJECTS)
	mkdir -p $(BINDIR)
	ld -T ../link.ld $(OBJECTS) $(SYSTEMLIB)/systemlib.a -o $(BINDIR)$(TARGET).elf

