#pragma once

#ifndef UNIT_TEST

#define C_BREAKPOINT() __asm("int $3");
#define OUTPORTB(val,port) asm volatile( "outb %0, %1" : : "a"((unsigned char)(val)), "Nd"((unsigned short)(port)) );
#define OUTPORTW(val,port) asm volatile( "outw %0, %1" : : "a"((unsigned short)(val)), "Nd"((unsigned short)(port)) );
#define OUTPORTL(val,port) asm volatile( "outl %0, %1" : : "a"((unsigned int)(val)), "Nd"((unsigned short)(port)) );
#define INPORTB(ret,port) asm volatile( "inb %1, %0" : "=a"((unsigned char)(ret)) : "Nd"((unsigned short)(port)) );
#define INPORTW(ret,port) asm volatile( "inw %1, %0" : "=a"((unsigned short)(ret)) : "Nd"((unsigned short)(port)) );
#define INPORTL(ret,port) asm volatile( "inl %1, %0" : "=a"(ret) : "Nd"((unsigned short)(port)) );
#define CLI(oldstatus) asm volatile ("pushf; pop %0; shr $9,%0; and $1,%0; cli" : "=g"(oldstatus));
#define STI(oldstatus) { if (oldstatus!=0) asm volatile ("sti");}
#define SWAP2(x) asm volatile("xchg %b0, %h0" : "=a" ((unsigned short)x) : "a" ((unsigned short)x));
#define SWAP4(x) asm volatile("bswap %0" : "=r" ((unsigned int)x) : "0" ((unsigned int)x));
#define SWAP6(x) asm volatile("bswap %0; shr $16,%0" : "=r" ((unsigned long)x) : "0" ((unsigned long)x));

#else

#define OUTPORTB(val,port) asm volatile( "nop" :: );
#define OUTPORTW(val,port) asm volatile( "nop" :: );
#define OUTPORTL(val,port) asm volatile( "nop" :: );
#define INPORTB(ret,port) asm volatile( "nop" :: );
#define INPORTW(ret,port) asm volatile( "nop" :: );
#define INPORTL(ret,port) asm volatile( "nop" :: );
#define CLI(oldstatus) asm volatile( "nop" :: );
#define STI(oldstatus) asm volatile( "nop" :: );
#define SWAP2(x) asm volatile( "nop" :: );
#define SWAP4(x) asm volatile( "nop" :: );
#define SWAP6(x) asm volatile( "nop" :: );

#endif

typedef struct
{
    unsigned long part1;
    unsigned long part2;
} spinlock_softirq_lock;
