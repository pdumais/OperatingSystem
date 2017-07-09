#include "../memorymap.h"
#include "macros.h"
#include "config.h"

.global getInterruptInfoForBus
.global ioAPICGetRedirect
.global initSMPForBST
.global initSMPForAP
.global initAPICTimer
.global ackAPIC
.global registerIRQ
.global sendIPI
.global registerIPIHandler
.global smpUp
.global get_apic_address

.extern markPageAsInUse
.extern schedulerHandler

#define APIC_BASE_MSR                       0x1B
#define APIC_REG_ID                         0x0020
#define APIC_REG_VERSION                    0x0030
#define APIC_REG_TASKPRIORITY               0x0080
#define APIC_REG_ARBITRATIONPRIORITY        0x0090    
#define APIC_REG_PROCESSORPRIORITY          0x00A0
#define APIC_REG_EOI                        0x00B0
#define APIC_REG_LOGICALDESTINATION         0x00D0
#define APIC_REG_DESTINATIONFORMAT          0x00E0
#define APIC_REG_SPURIOUSINTERRUPTVECTOR    0x00F0
#define APIC_REG_ISR                        0x0100
#define APIC_REG_TMR                        0x0180
#define APIC_REG_IRR                        0x0200
#define APIC_REG_ERRORSTATUS                0x0280
#define APIC_REG_INTERRUPTCOMMANDLOW        0x0300
#define APIC_REG_INTERRUPTCOMMANDHIGH       0x0310
#define APIC_REG_LOCALVECTORTABLETIMER      0x0320
#define APIC_REG_PERFORMANCECOUNTERLVT      0x0340
#define APIC_REG_LOCALVECTORTABLELINT0      0x0350
#define APIC_REG_LOCALVECTORTABLELINT1      0x0360
#define APIC_REG_LOCALVECTORTABLEERROR      0x0370
#define APIC_REG_TIMERINITIALCOUNT          0x0380
#define APIC_REG_TIMERCURRENTCOUNT          0x0390
#define APIC_REG_TIMERDIVIDECONFIGURATION   0x03E0

// The multiplication by 64k is just to avoid getting a floating pointer. We
// will need to div (or shr 16) after multiplying
#define APIC_TICKS_PER_MS_MULTIPLIER ((DESIRED_APIC_PERIOD_NS*65536)/54925500)



//
// The IOAPIC reserves 64 interrupt vectors for its handlers (int32-95)
// each vector can be shared with with 16 IRQ handlers.
//
#define DECLAREIRQHANDLER(irq) intIRQ##irq:; \
        push    %rax; \
        mov     $(IOAPICHANDLERS+(irq*16*8)),%rax; \
        jmp     baseIRQHandler; \

#define REGISTERINTFORIRQ(int,irq) mov $int,%rax; \
    mov     $intIRQ##irq,%rbx; \
    call    registerInterruptGate; \

#define DECLAREIPIHANDLER(vector) intIPI##vector:; \
        push    %rax; \
        mov     $IPI_FIRST_VECTOR+vector,%rax; \
        jmp     baseIPIHandler; \

// Note: It is EXTREMELY important that IPI handlers be registered 
//       as interrupt gates, as opposed to trap gates. Interrupt gates
//       will disable interrupts on entry, preventing preemption.
//       if a IPI handler would be preempted, then scheduled on 
//       another CPU, it would defeat the whole point of sending IPIs
#define REGISTERINTFORIPI(vector) mov $IPI_FIRST_VECTOR+vector,%rax; \
    mov     $intIPI##vector,%rbx; \
    call    registerInterruptGate


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: initSMPForBST()
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
initSMPForBST:
    push    %rdi
    push    %rax
    push    %rcx
    push    %rdx
    
    // clear IPI message blocks
    mov     $IPI_MESSAGES,%rdi
    mov     $(IPI_LAST_VECTOR-IPI_FIRST_VECTOR+1)*16,%rcx
    xor     %rax,%rax
    rep     stosb

    // clear the IOAPIC irq handlers
    mov     $IOAPICHANDLERS,%rdi
    xor     %rax,%rax
    mov     $128,%rcx
    rep     stosq

    call    parseMPTables
    cmp     $1,(%rax)           // if there is 1 cpu or MP was not found
    jbe     exitInitSMP         // then exit the function

    //setup spurious interrupt for APIC
    mov     $APIC_SPURIOUS_VECTOR,%rax
    mov     $spuriousHandler,%rbx
    call    registerInterruptGate
    //setup error interrupt for APIC
    mov     $APIC_ERROR_VECTOR,%rax
    mov     $errorHandler,%rbx
    call    registerInterruptGate

    REGISTERINTFORIRQ(32,0)
    REGISTERINTFORIRQ(33,1)
    REGISTERINTFORIRQ(34,2)
    REGISTERINTFORIRQ(35,3)
    REGISTERINTFORIRQ(36,4)
    REGISTERINTFORIRQ(37,5)
    REGISTERINTFORIRQ(38,6)
    REGISTERINTFORIRQ(39,7)
    REGISTERINTFORIRQ(40,8)
    REGISTERINTFORIRQ(41,9)
    REGISTERINTFORIRQ(42,10)
    REGISTERINTFORIRQ(43,11)
    REGISTERINTFORIRQ(44,12)
    REGISTERINTFORIRQ(45,13)
    REGISTERINTFORIRQ(46,14)
    REGISTERINTFORIRQ(47,15)
    REGISTERINTFORIRQ(48,16)
    REGISTERINTFORIRQ(49,17)
    REGISTERINTFORIRQ(50,18)
    REGISTERINTFORIRQ(51,19)
    REGISTERINTFORIRQ(52,20)
    REGISTERINTFORIRQ(53,21)
    REGISTERINTFORIRQ(54,22)
    REGISTERINTFORIRQ(55,23)
    REGISTERINTFORIRQ(56,24)
    REGISTERINTFORIRQ(57,25)
    REGISTERINTFORIRQ(58,26)
    REGISTERINTFORIRQ(59,27)
    REGISTERINTFORIRQ(60,28)
    REGISTERINTFORIRQ(61,29)
    REGISTERINTFORIRQ(62,30)
    REGISTERINTFORIRQ(63,31)
    REGISTERINTFORIRQ(64,32)
    REGISTERINTFORIRQ(65,33)
    REGISTERINTFORIRQ(66,34)
    REGISTERINTFORIRQ(67,35)
    REGISTERINTFORIRQ(68,36)
    REGISTERINTFORIRQ(69,37)
    REGISTERINTFORIRQ(70,38)
    REGISTERINTFORIRQ(71,39)
    REGISTERINTFORIRQ(72,40)
    REGISTERINTFORIRQ(73,41)
    REGISTERINTFORIRQ(74,42)
    REGISTERINTFORIRQ(75,43)
    REGISTERINTFORIRQ(76,44)
    REGISTERINTFORIRQ(77,45)
    REGISTERINTFORIRQ(78,46)
    REGISTERINTFORIRQ(79,47)
    REGISTERINTFORIRQ(80,48)
    REGISTERINTFORIRQ(81,49)
    REGISTERINTFORIRQ(82,50)
    REGISTERINTFORIRQ(83,51)
    REGISTERINTFORIRQ(84,52)
    REGISTERINTFORIRQ(85,53)
    REGISTERINTFORIRQ(86,54)
    REGISTERINTFORIRQ(87,55)
    REGISTERINTFORIRQ(88,56)
    REGISTERINTFORIRQ(89,57)
    REGISTERINTFORIRQ(90,58)
    REGISTERINTFORIRQ(91,59)
    REGISTERINTFORIRQ(92,60)
    REGISTERINTFORIRQ(93,61)
    REGISTERINTFORIRQ(94,62)
    REGISTERINTFORIRQ(95,63)

    // The number is the index starting from IPI_FIRST_VECTOR
    REGISTERINTFORIPI(0)
    REGISTERINTFORIPI(1)
    REGISTERINTFORIPI(2)
    REGISTERINTFORIPI(3)
    REGISTERINTFORIPI(4)
    REGISTERINTFORIPI(5)
    REGISTERINTFORIPI(6)
    REGISTERINTFORIPI(7)
    REGISTERINTFORIPI(8)
    REGISTERINTFORIPI(9)
    REGISTERINTFORIPI(10)
    REGISTERINTFORIPI(11)
    REGISTERINTFORIPI(12)
    REGISTERINTFORIPI(13)
    REGISTERINTFORIPI(14)
    REGISTERINTFORIPI(15)
    REGISTERINTFORIPI(16)
    REGISTERINTFORIPI(17)
    REGISTERINTFORIPI(18)
    REGISTERINTFORIPI(19)
    REGISTERINTFORIPI(20)
    REGISTERINTFORIPI(21)
    REGISTERINTFORIPI(22)
    REGISTERINTFORIPI(23)
    REGISTERINTFORIPI(24)
    REGISTERINTFORIPI(25)
    REGISTERINTFORIPI(26)
    REGISTERINTFORIPI(27)
    REGISTERINTFORIPI(28)
    REGISTERINTFORIPI(29)
    REGISTERINTFORIPI(30)
    REGISTERINTFORIPI(31)

    // Disable PIC
    mov     $0xFF,%al
    out     %al,$0xA1
    out     %al,$0x21

    call    enableAPIC

    // redirect PIC to APIC
    mov     $0x70,%al
    out     %al,$0x22
    mov     $0x1,%al
    out     %al,$0x23

    call    calibrateAPICTimer

    //setup timer interrupt for APIC
    mov     $APIC_TIMER_VECTOR,%rax
    mov     $apic_timer_handler,%rbx
    call    registerInterruptGate

    GET_APIC_ID(%eax)
    mov     $STARTEDCPUS,%rdi
    btsq    %rax,(%rdi)

    mov     $APIC_BASE,%rcx
    mov     $APIC_BASE,%rax
    mov     $PAGETABLES,%rdi
    call    markPageAsInUse     // make sure it wont be used in heap

    call    startAllCPUs
exitInitSMP:
    pop     %rdx
    pop     %rcx
    pop     %rax
    pop     %rdi
    ret
  
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: initAP()
// This is for the Application Processors
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
initSMPForAP:
    push    %rdi
    push    %rax

    call    enableAPIC

    GET_APIC_ID(%eax)
    mov     $STARTEDCPUS,%rdi
    btsq    %rax,(%rdi)

    pop     %rax
    pop     %rdi
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: parseMPTables()
// returns rax=address of CPUINFO
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
calibrateAPICTimer:
    push    %r8
    push    %rax

    //register interrupt for getting the one shot timer
    mov     $64,%rax 
    mov     $oneShotCalibrationHandler,%rbx
    call    registerInterruptGate
    mov     $2,%rdi         //TODO: is it always going to be irq2 on ioapic?
    mov     $64,%rsi
    call    ioAPICRedirect

    // enable APIC Timer in one shot mode with divisor or 16 and countdown 0f 0x10000
    mov     $3,%rax
    mov     %eax,APIC_BASE+APIC_REG_TIMERDIVIDECONFIGURATION
    mov     $0x00|APIC_TIMER_VECTOR,%rax        //one-shot mode
    mov     %eax,APIC_BASE+APIC_REG_LOCALVECTORTABLETIMER
    
    // enable ints and wait for apic timer to tick
    mov     $0x00, %al // mode0, channel 0, accessmode:latch
    outb    %al,$0x43
    mov     $0xFF, %al
    outb    %al,$0x40
    outb    %al,$0x40

    // set the APIC timer count
    mov     $0xFFFFFFFF1,%rax
    mov     %eax,APIC_BASE+APIC_REG_TIMERINITIALCOUNT
    sti
1:  cmp     $0,calibCountWait
    je      1b
    cli
    mov     APIC_BASE+APIC_REG_TIMERCURRENTCOUNT,%eax
    mov     $0xFFFFFFFF,%r8
    sub     %rax,%r8

    mov     $2,%rdi
    call    ioAPICDisableRedirect

    mov     %r8,calibCount
    pop     %rax
    pop     %r8
    ret

oneShotCalibrationHandler:
    movq    $1,calibCountWait
    call    ackAPIC
    iretq

calibCount: .quad 0
calibCountWait: .quad 0

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: parseMPTables()
// returns rax=address of CPUINFO
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
parseMPTables:
    push    %rsi
    push    %rdi
    push    %rcx
    push    %rdx

    /////////////////////////////////////////////
    // We will search for the MP structure
    // in the first 1kb at 9FC00 and in the
    // 64k block at F0000. If we can't find it 
    // there then we assume that there only
    // 1 CPU
    /////////////////////////////////////////////
    mov     $0x0009FC00,%rdi    // search EBDA first
    mov     $1024,%rcx
    call    searchMP
    cmp     $0,%rax
    jne     foundMP
    
    mov     $0xF0000,%rdi       // search BIOS ROM 
    mov     $0x10000,%rcx
    call    searchMP
    cmp     $0,%rax
    jne     foundMP
    jmp     exitParseMPTables


    /////////////////////////////////////////////
    // At this point, MP structure was found
    // address is in rax
    // Note: writing in eax clears the higher 
    //       32bit automatically.
    /////////////////////////////////////////////
foundMP:
    xor     %rcx,%rcx
    mov     4(%rax),%eax        // 2nd dword in the struct is the address of MP configuration table. 
    mov     34(%rax),%cx        // entry count
    and     $0xFFFF,%rcx
    mov     36(%rax),%ebx       // APIC base address
    mov     %rbx,APIC_ADDRESS
    xor     %rbx,%rbx
    add     $44,%rax            // 1st entry, right after the end of table


    /////////////////////////////////////////////
    // Find all CPUs in the list
    // Entries are sorted by ascending order. 
    // So CPUs will be listed first
    /////////////////////////////////////////////
    push    %r9
    push    %r10
    push    %r11
    push    %r12
    mov     $CPUINFO,%r9
    mov     $IOAPICINFO,%r10
    mov     $BUSINFO,%r11
    mov     $INTSINFO,%r12

entryLoop:    
    cmp     $0,%rcx
    je      noMoreEntries    
    dec     %rcx
    mov     (%rax),%rbx
    add     $8,%rax
    cmp     $0,%bl
    je      cpuEntry
    cmp     $1,%bl
    je      busEntry
    cmp     $2,%bl
    je      ioApicEntry
    cmp     $3,%bl
    je      intsEntry
    jmp     entryLoop

cpuEntry:
    add     $12,%rax        // those entries are 20bytes long
    bt      $24,%rbx          // CPU enabled? flag is at 3rd byte
    jnc     entryLoop
    incq    CPUCOUNT
    shr     $8,%rbx
    mov     %bl,(%r9)        // store local APIC ID in CPUINFO structure
    add     $1,%r9
    jmp     entryLoop

busEntry:
    mov     %rbx,%rdx
    shr     $8,%edx
    and     $0xFF,%edx
    mov     %edx,(%r11)
    shr     $16,%rbx
    mov     %ebx,8(%r11)
    add     $16,%r11
    jmp     entryLoop

intsEntry:
    shr     $32,%rbx
    mov     %ebx,(%r12)
    add     $4,%r12
    jmp     entryLoop

ioApicEntry:
    bt      $24,%ebx          // IOAPIC enabled? 
    jnc     entryLoop
    mov     %rbx,%rdx
    shr     $8,%rdx
    and     $0xFF,%rdx
    mov     %edx,(%r10)       // Store the IO APIC ID in IOAPICINFO struct
    shr     $32,%rbx        
    mov     %ebx,8(%r10)      // Store the IO APIC address in IOAPICINFO struct
    
    cmp     $0,IOAPICCOUNT
    je      1f  
    int     $3                // We don't support more than 1 IOAPIC yet

1:  incq    IOAPICCOUNT       // increase IOAPIC count
    add     $16,%r10
    jmp     entryLoop

noMoreEntries:
    pop     %r12
    pop     %r11
    pop     %r10
    pop     %r9
exitParseMPTables:
    mov     $CPUINFO,%rax
    pop     %rdx
    pop     %rcx
    pop     %rdi
    pop     %rsi
    ret



////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: enableAPIC()
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
enableAPIC:
    push    %rdi
    push    %rax

    mov     $APIC_BASE,%rdi

    // unmask spurious and enable APIC
    mov     $(APIC_SPURIOUS_VECTOR | 0x100), %rax   // OR with enable flag
    mov     %eax,APIC_REG_SPURIOUSINTERRUPTVECTOR(%rdi)

    // Mask all vectors first
    mov     $0x10000,%rax
    mov     %eax,APIC_REG_LOCALVECTORTABLEERROR(%rdi)
    mov     %eax,APIC_REG_LOCALVECTORTABLETIMER(%rdi)
    mov     %eax,APIC_REG_PERFORMANCECOUNTERLVT(%rdi)
    mov     %eax,APIC_REG_LOCALVECTORTABLELINT0(%rdi)
    mov     %eax,APIC_REG_LOCALVECTORTABLELINT1(%rdi)
    xor     %rax,%rax
    mov     %eax,APIC_REG_TASKPRIORITY(%rdi)

    // unmask error vector
    mov     $APIC_ERROR_VECTOR,%rax                                          
    mov     %eax,APIC_REG_LOCALVECTORTABLEERROR(%rdi)

    pop     %rax
    pop     %rdi
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: startAllCPUs(rdi=address of boot code)
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
startAllCPUs:
    push    %rdi
    push    %rax
    push    %rcx
    push    %rbx
    push    %rdx

    mov     $APIC_BASE,%rdi
    mov     $0xC4500, %rax   // broadcast INIT to all APs
    mov     %eax, APIC_REG_INTERRUPTCOMMANDLOW(%rdi)
    WAIT(100000000) //1 billioon loop should take more than 10ms on a 4ghz CPU
    mov     $0xC4600, %rax   // broadcast SIPI to all APs
    mov     $SMP_TRAMPOLINE,%rcx
    shr     $12,%rcx
    and     $0xFF,%rcx
    or      %rcx,%rax

    mov     1000,%rcx

1:  dec     %rcx
    jz      cantStartAllCPUs
    mov     %eax, APIC_REG_INTERRUPTCOMMANDLOW(%rdi)
    WAIT(100000000) //1 billioon loop should take more than 10ms on a 4ghz CPU

    popcnt  STARTEDCPUS,%rdx
    cmp     CPUCOUNT,%rdx
    jnz      1b

    pop     %rdx
    pop     %rbx
    pop     %rcx
    pop     %rax
    pop     %rdi
    ret
cantStartAllCPUs:
    //TODO: it is rather drastic to halt everything because of that
    STALL()

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: initSMP(rdi=startAddress,rcx=size_of_area_to_search)
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
searchMP:
    mov     $0x5f504d5f,%rax    // "_MP_"
1:  cmp     %eax,(%rdi)
    je      2f
    inc     %rdi
    loop    1b
    xor     %rax,%rax
    ret
2:  mov     %rdi,%rax
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: initAPICTimer
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
initAPICTimer:
    push    %r8
    call    getTimerValues  
    mov     $APIC_BASE,%r8
    mov     %eax,APIC_REG_TIMERDIVIDECONFIGURATION(%r8)
    mov     $0x20000|APIC_TIMER_VECTOR,%rax                                          // Interrupt vector 20h with Enable flag
    mov     %eax,APIC_REG_LOCALVECTORTABLETIMER(%r8)

    // NOTE: Timer only works if writing initial value AFTER enabling timer
    mov     %edi,APIC_REG_TIMERINITIALCOUNT(%r8)
    pop     %r8
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: getTimerValues
// Return: %rax = divider, %rdi = count
// Divider may be: 
//          0: Div by 2
//          1: Div by 4
//          2: Div by 8
//          3: Div by 16
//          8: Div by 32
//          9: Div by 64
//          A: Div by 128
//          B: Div by 1
// Intel Manual 3 Section 10.5.4
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
getTimerValues:
    push    %rdx
    push    %rcx
    xor     %rdx,%rdx
    mov     calibCount,%rcx
    mov     $APIC_TICKS_PER_MS_MULTIPLIER,%rax
    mul     %rcx
    mov     %rax,%rdi
    shr     $16,%rdi
    mov     $3,%rax
    pop     %rcx
    pop     %rdx
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: ackAPIC
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
ackAPIC:
    push    %rax
    mov     $1,%rax
    mov     %eax,APIC_BASE+APIC_REG_EOI
    pop     %rax
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: ioAPICRead(rdi=ioApicID, esi=register)
// Return: eax=value
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
ioAPICRead:
    push    %rdi
    shl     $4,%rdi
    add     $IOAPICINFO+8,%rdi
    mov     (%rdi),%rdi

    mov     %esi,(%rdi)
    mov     16(%rdi),%eax
    pop     %rdi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: ioAPICWrite(rdi=ioApicID, esi=register,edx=value)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
ioAPICWrite:
    push    %rdi
    shl     $4,%rdi
    add     $IOAPICINFO+8,%rdi
    mov     (%rdi),%rdi
    mov     %esi,(%rdi)
    mov     %edx,16(%rdi)
    pop     %rdi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: ioAPICRedirect(rdi=entry, esi=vector)
//  Sets a redirection at IOAPIC index to cpu vector defined in esi
//  WARNING: we only support the first apic in here. 
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
ioAPICRedirect:
    push    %rax
    push    %rdx
    push    %rdi

    mov     %rsi,%rdx
    mov     %rdi,%rsi
    // entry register = 0x10+index*2
    lea     0x10(,%rsi,2),%rsi

    // set the redirection entry. bits7:0 are the vector
    //unmasked, physical deliverymode, fixed delivery mode,  Deliver on cpu0
    //TODO: should provide a way to specify polarity and trigger mode
    //or      $(1<<15)|(1<<13),%rdx

    mov     $0,%rdi
    call    ioAPICWrite

    add     $1,%rsi
    shr     $32,%rdx
    call    ioAPICWrite

    pop     %rdi
    pop     %rdx
    pop     %rax
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: ioAPICDisabledRedirect(rdi=entry)
//  Sets a redirection at IOAPIC index to cpu vector defined in esi
//  WARNING: we only support the first apic in here.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
ioAPICDisableRedirect:
    push    %rdx
    push    %rdi
    push    %rsi

    mov     %rdi,%rsi
    // entry register = 0x10+index*2
    lea     0x10(,%rsi,2),%rsi

    mov     $0x10000,%rdx       // mask flag
    mov     $0,%rdi
    call    ioAPICWrite

    add     $1,%rsi
    shr     $32,%rdx
    call    ioAPICWrite

    pop     %rsi
    pop     %rdi
    pop     %rdx
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: ioAPICGetRedirect(rdi=entry)
//  Gets a redirection at IOAPIC index.
//  WARNING: we only support the first apic in here. 
// return:rax= redirect entry
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
ioAPICGetRedirect:
    push    %rdi
    push    %rsi

    // entry register = 0x10+index*2
    mov     %rdi,%rsi 
    lea     0x10(,%rsi,2),%rsi

    mov     $0,%rdi
    call    ioAPICRead
    push    %rax
    add     $1,%rsi
    call    ioAPICRead
    shl     $32,%rax
    pop     %rdi
    or      %rdi,%rax

    pop     %rsi
    pop     %rdi
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: getInterruptInfoForBus(rdi=busname ie "PCI ", rsi=buffer)
// The buffer parameter receives an array of 32bit integers defined as follow:
//  byte0: destination IO APIC int#
//  byte1: source bus IRQ
//  byte2: bus number
//
// Note that because of the "barber pole" effect in the PCI int lines, a device's
// point of view of the int line it is using might not be the same as the bus 
// point of view (ie. device emits on PINA but PINB is connected to device's PINA)
//
// This function reports the int number from the device's point of vue.
///////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
getInterruptInfoForBus:
    push    %rcx
    push    %rax
    push    %rbx
    push    %rdx
    push    %r8

    mov     $BUSINFO,%rax
    mov     $64,%rcx
1:  mov     8(%rax),%rbx
    cmp     %rbx,%rdi
    jnz     3f
    call    busFound
3:  add     $16,%rax
    loop    1b

    movl    $0,(%rsi)
    pop     %r8
    pop     %rdx
    pop     %rbx
    pop     %rax
    pop     %rcx
    ret

busFound:
    mov     (%rax),%rax
    // At this point, rax contains the bus id
    mov     $INTSINFO,%rbx
    mov     $64,%r8
1:  mov     (%rbx),%edx
    add     $4,%rbx
    cmp     %dl,%al
    jne     2f
    
    mov     %dl,3(%rsi)
    mov     %dh,1(%rsi)
    shr     $16,%rdx
    mov     %dh,(%rsi)
    add     $4,%rsi

2:  dec     %r8
    jnz     1b
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: Spurious Int Handler
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
spuriousHandler:    
    iretq

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: Error Int Handler
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
errorHandler:    
    call    ackAPIC
    iretq

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: registerIRQ(rdi=handler, rsi=IOAPIC irq number)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
registerIRQ:
    push    %rsi
    push    %rdi
    push    %rax
    push    %rcx
    push    %r8

    // Add handler in handlers table. There are 16 handler slot per IRQ
    // each entry is 64bit so first slot = IOAPICHANDLERS+(irq*(8*16))
    mov     %rsi,%rax
    shl     $7,%rax                         // *8*16
    add     $IOAPICHANDLERS,%rax
    mov     $16,%rcx
    mov     %rax,%r8    

    // First check if handler already registered
1:  cmp     %rdi,(%r8)
    je      exitRegisterIRQ
    add     $8,%r8
    loop    1b

    mov     $16,%rcx
1:  cmp     $0,(%rax)
    jz      2f    
    add     $8,%rax
    loop    1b
    int     $3                              // table was full
    jmp     exitRegisterIRQ
2:  mov     %rdi,(%rax)
    cmp     $16,%rcx
    jne     exitRegisterIRQ                 //if rcx==0, it means it is the first
                                            // entry we register. So setup the 
                                            // ioapic. Otherwise, it was already 
                                            // done before
    mov     %rsi,%rdi
    add     $32,%rsi
    call    ioAPICRedirect

exitRegisterIRQ:
    pop     %r8
    pop     %rcx
    pop     %rax
    pop     %rdi
    pop     %rsi
    ret 

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: baseIRQHandler
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
baseIRQHandler:
    PUSHAEXCEPTRAX              // rax was saved before going here

    mov     $16,%rcx
1:  cmp     $0,(%rax)
    jz      baseIRQHandlerExit  // no more handlers, exit
    push    %rax                // There is no guarantee to handler will preserve registers
    push    %rcx
    call    (%rax)              
    pop     %rcx
    pop     %rax
    add     $8,%rax
    loop    1b
baseIRQHandlerExit:
    call    ackAPIC       //TODO: should indicate which IOAPIC

    POPA
    iretq

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// IRQ handlers
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
DECLAREIRQHANDLER(0)
DECLAREIRQHANDLER(1)
DECLAREIRQHANDLER(2)
DECLAREIRQHANDLER(3)
DECLAREIRQHANDLER(4)
DECLAREIRQHANDLER(5)
DECLAREIRQHANDLER(6)
DECLAREIRQHANDLER(7)
DECLAREIRQHANDLER(8)
DECLAREIRQHANDLER(9)
DECLAREIRQHANDLER(10)
DECLAREIRQHANDLER(11)
DECLAREIRQHANDLER(12)
DECLAREIRQHANDLER(13)
DECLAREIRQHANDLER(14)
DECLAREIRQHANDLER(15)
DECLAREIRQHANDLER(16)
DECLAREIRQHANDLER(17)
DECLAREIRQHANDLER(18)
DECLAREIRQHANDLER(19)
DECLAREIRQHANDLER(20)
DECLAREIRQHANDLER(21)
DECLAREIRQHANDLER(22)
DECLAREIRQHANDLER(23)
DECLAREIRQHANDLER(24)
DECLAREIRQHANDLER(25)
DECLAREIRQHANDLER(26)
DECLAREIRQHANDLER(27)
DECLAREIRQHANDLER(28)
DECLAREIRQHANDLER(29)
DECLAREIRQHANDLER(30)
DECLAREIRQHANDLER(31)
DECLAREIRQHANDLER(32)
DECLAREIRQHANDLER(33)
DECLAREIRQHANDLER(34)
DECLAREIRQHANDLER(35)
DECLAREIRQHANDLER(36)
DECLAREIRQHANDLER(37)
DECLAREIRQHANDLER(38)
DECLAREIRQHANDLER(39)
DECLAREIRQHANDLER(40)
DECLAREIRQHANDLER(41)
DECLAREIRQHANDLER(42)
DECLAREIRQHANDLER(43)
DECLAREIRQHANDLER(44)
DECLAREIRQHANDLER(45)
DECLAREIRQHANDLER(46)
DECLAREIRQHANDLER(47)
DECLAREIRQHANDLER(48)
DECLAREIRQHANDLER(49)
DECLAREIRQHANDLER(50)
DECLAREIRQHANDLER(51)
DECLAREIRQHANDLER(52)
DECLAREIRQHANDLER(53)
DECLAREIRQHANDLER(54)
DECLAREIRQHANDLER(55)
DECLAREIRQHANDLER(56)
DECLAREIRQHANDLER(57)
DECLAREIRQHANDLER(58)
DECLAREIRQHANDLER(59)
DECLAREIRQHANDLER(60)
DECLAREIRQHANDLER(61)
DECLAREIRQHANDLER(62)
DECLAREIRQHANDLER(63)

DECLAREIPIHANDLER(0)
DECLAREIPIHANDLER(1)
DECLAREIPIHANDLER(2)
DECLAREIPIHANDLER(3)
DECLAREIPIHANDLER(4)
DECLAREIPIHANDLER(5)
DECLAREIPIHANDLER(6)
DECLAREIPIHANDLER(7)
DECLAREIPIHANDLER(8)
DECLAREIPIHANDLER(9)
DECLAREIPIHANDLER(10)
DECLAREIPIHANDLER(11)
DECLAREIPIHANDLER(12)
DECLAREIPIHANDLER(13)
DECLAREIPIHANDLER(14)
DECLAREIPIHANDLER(15)
DECLAREIPIHANDLER(16)
DECLAREIPIHANDLER(17)
DECLAREIPIHANDLER(18)
DECLAREIPIHANDLER(19)
DECLAREIPIHANDLER(20)
DECLAREIPIHANDLER(21)
DECLAREIPIHANDLER(22)
DECLAREIPIHANDLER(23)
DECLAREIPIHANDLER(24)
DECLAREIPIHANDLER(25)
DECLAREIPIHANDLER(26)
DECLAREIPIHANDLER(27)
DECLAREIPIHANDLER(28)
DECLAREIPIHANDLER(29)
DECLAREIPIHANDLER(30)
DECLAREIPIHANDLER(31)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// sendIPI(rdi=vector, rsi=Data,  %dl=messageID)
//
// The IPIMessage structture is a 128bit structure with the following format:
// 127:64 : 64bit Data
// 63:32 :  RESERVED
// 31:16 :  MessageID
// 15:8 :   CPU count
// 7:1 :    RESERVED
// 0:0 :    lock bit. Used to lock the message when trying send
// There is one IPIMessage block for each vector that we can trigger. The list
// of IPIMessage resides at IPIMESSAGES (see memory.h). The list contains 32
// IPIMessage blocks, allowing a total of 32 different interrupts that can be sent
// to other CPUs.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
sendIPI:
    bt          $0,SMPISUP
    jnc         4f

    push        %rdi
    push        %rax
    push        %rbx
    push        %rdx
    mov         %rdi,%rbx
    
    // 1) Find the IPIMessage block associated with the requested vector
    // rdi = IPI_MESSAGES+((rdi-first_vector)*16)
    shl         $4,%rdi
    add         $IPI_MESSAGES-(IPI_FIRST_VECTOR*16),%rdi

    // 2) Attempt to lock the block so that no one else sends at the same time
1:  bts         $0,(%rdi)
    jc          1b

    // 3) Fill in the MessageID and Data
    mov         %dl,2(%rdi)
    mov         %rsi,8(%rdi)

    // 4) Set the number of CPUS we are targeting. That would be the cpuCount
    //    minus ourself
    popcnt      STARTEDCPUS,%rdx
    dec         %rdx
    mov         %dl,1(%rdi)

    // 5) Disable interrupts locally and wait for local APIC to be idle
    pushfq
    cli
    mov         $APIC_BASE,%rax
1:  btl         $12,APIC_REG_INTERRUPTCOMMANDLOW(%rax)
    jc          1b
    popfq

    // 6) send the IPI
    or          $(3<<18),%rbx // all CPUS except this one
    mov         %ebx,APIC_REG_INTERRUPTCOMMANDLOW(%rax)

    // 7) wait for targeted CPU count to fall to 0. If the other CPUS
    //    have disabled interrupts, this could take a lot of time
    //    to accomplish
1:  cmpb        $0,1(%rdi)
    jne         1b

    // 8) Release the lock
    btr         $0,(%rdi)

    pop         %rdx
    pop         %rbx
    pop         %rax
    pop         %rdi
4:  ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// ackIPI(rdi=vector)
// Before calling ackIPI, a process should first retrieve the message using
// getIPIMEssage and process it. The AP should guarantee that the messages
// is done processing when acking. 
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
ackIPI:
    push        %rdi

    // rdi = IPI_MESSAGES+((rdi-first_vector)*16)
    shl         $4,%rdi
    add         $IPI_MESSAGES-(IPI_FIRST_VECTOR*16),%rdi

    // 1) decrease the CPU count in the IPIMessage block associated with vector
    lock decb   1(%rdi)

    call        ackAPIC
    pop         %rdi
    ret
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// getIPIMessage(rax=vector)
// // RETURN: rdi=message data, rsi=messageID
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
getIPIMessage:
    push        %rax

    cmp         $IPI_FIRST_VECTOR,%rax
    jb          getIPIMessageError
    cmp         $IPI_LAST_VECTOR,%rax
    ja          getIPIMessageError

    // rax = IPI_MESSAGES+((rax-first_vector)*16)
    shl         $4,%rax               
    add         $IPI_MESSAGES-(IPI_FIRST_VECTOR*16),%rax
    
    mov         2(%rax),%rsi
    mov         8(%rax),%rdi
    and         $0xFF,%rsi

    pop         %rax
    ret
getIPIMessageError:
    int         $3

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: baseIPIHandler
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
baseIPIHandler:
    PUSHAEXCEPTRAX              // rax was saved before going here
    push        %rax

    // rax was set to vector number before going here
    call        getIPIMessage
    // now rdi contains the data

    shl         $3,%rax                                 //handler entries are 64bit
    add         $IPI_HANDLERS-(IPI_FIRST_VECTOR*8),%rax
    mov         (%rax),%rax
    cmp         $0,%rax
    je          1f

    push        %rax
    GET_APIC_ID(%eax)
    mov         %eax,%edx
    pop         %rax
    // At this point, rdi=message data, rsi=messageID. edx=APICID
    // They will be passed as parameter to handler
    call        %rax

1:  pop         %rdi
    call        ackIPI   
    POPA
    iretq

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: registerIPIHandler(rdi=vector_number, rsi=handler_address)
//
// The vector number is the absolute value. ie: don't use 0, use IPI_FIRST_VECTOR
//
// The handler should have the signature: handler(rdi=data,rsi=msgID,rdx=ApicID)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
registerIPIHandler:
    push        %rdi

    shl         $3,%rdi                                 //handler entries are 64bit
    add         $IPI_HANDLERS-(IPI_FIRST_VECTOR*8),%rdi
    mov         %rsi,(%rdi)

    pop         %rdi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: smpUp()
// set the SMPISUP flag to tell rest of system that SMP is up
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
smpUp:
    bts         $0,SMPISUP
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: get_apic_address()
// returns the APIC physical address 
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
get_apic_address:
    push        %rcx
    push        %rdx
    mov         $APIC_BASE_MSR,%rcx
    rdmsr
    shr         $12,%rax
    shl         $12,%rax
    pop         %rdx
    pop         %rcx
    ret
    

////////////////////////////////////////////////////////////////////////////////////
// DATA section
////////////////////////////////////////////////////////////////////////////////////
.align 16
APIC_ADDRESS: .quad 0x0
CPUCOUNT: .quad 0x00
CPUINFO:
    .rept 64    // structure handles max 64 CPUs
    .byte 0     // local apic ID
    .endr
IOAPICCOUNT: .quad 0x00
IOAPICINFO:
    .rept 64    // structure handles max 64 CPUs
    .quad 0     // local apic ID
    .quad 0     // IO APIC address
    .endr
BUSINFO:
    .rept 64   
    .quad 0     
    .quad 0     
    .endr
INTSINFO:
    .rept 64   
    .long 0     
    .endr

STARTEDCPUS: .quad 0x00 //bit field for up to 64 cpus
SMPISUP: .quad 0x00
IPI_HANDLERS:
    .rept (IPI_LAST_VECTOR-IPI_FIRST_VECTOR+1)
    .quad 0
    .endr
    









