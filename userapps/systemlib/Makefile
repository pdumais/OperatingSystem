TARGET=libc
SOURCES=$(wildcard *c)
OBJECTS=$(SOURCES:%.c=%.o)

all: $(TARGET).a

$(OBJECTS): %.o : %.c
	gcc -ffreestanding  -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-3dnow -fno-exceptions -c $< -o $@ -I../../kernel/includes


$(TARGET).a: $(OBJECTS)
	ar rcs systemlib.a $(OBJECTS)

clean:
	-rm *.o
	-rm *.a
