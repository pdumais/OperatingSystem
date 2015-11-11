#define OUTPORTB(val,port) asm volatile( "outb %0, %1" : : "a"((unsigned char)(val)), "Nd"((unsigned short)(port)) );
#define OUTPORTW(val,port) asm volatile( "outw %0, %1" : : "a"((unsigned short)(val)), "Nd"((unsigned short)(port)) );
#define OUTPORTL(val,port) asm volatile( "outl %0, %1" : : "a"((unsigned int)(val)), "Nd"((unsigned short)(port)) );
#define INPORTB(ret,port) asm volatile( "inb %1, %0" : "=a"((unsigned char)(ret)) : "Nd"((unsigned short)(port)) );
#define INPORTW(ret,port) asm volatile( "inw %1, %0" : "=a"((unsigned short)(ret)) : "Nd"((unsigned short)(port)) );
#define INPORTL(ret,port) asm volatile( "inl %1, %0" : "=a"(ret) : "Nd"((unsigned short)(port)) );

#define SWAP2(x) asm volatile("xchg %b0, %h0" : "=a" ((unsigned short)x) : "a" ((unsigned short)x));
#define SWAP4(x) asm volatile("bswap %0" : "=r" ((unsigned int)x) : "0" ((unsigned int)x));
#define SWAP6(x) asm volatile("bswap %0; shr $16,%0" : "=r" ((unsigned long)x) : "0" ((unsigned long)x));
#define YIELD() asm volatile("int $0x40");
