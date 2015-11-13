#include "../memorymap.h"

.global malloc
.global init_heap

////////////////////////////////////////////////////////////////////////////////////
// Model:
//  heap memory is managed using a linked list of blocks. Each block start
//  with a header called the Memory Allocation Entry (MAE). A MAE is
//  128bit wide and structured as follow:
//      127:124: reserved
//      123:68:  address of next block
//      67:1:    reserved
//      0:0:     1=in use, 0=free
// 
//  Each block are aligned on a 16 byte boundary. Hence why the "next block"
//  address's lower 4bits are not used. This means that memory is allocated in
//  16bytes multiples. The MAE occupies the first 16byte of the block.
//  So a request for a buffer of 1 byte, would create a block of 32 byte: 16 for
//  the MAE and then 16 for the buffer itself. The memory manager guarantees
//  that each allocated buffers are aligned on a 16byte boundary.
// 
//  Initially, on process creation, the heap only contains one big free block.
//  Its MAE "inuse" field is set to 0 (free), and the "next" field points to the
//  end of the heap. The end of the heap is defined as the last virtual address
//  that can be used by the heap, and is at the same time, the bottom of the 
//  stack
// 
//  Note that since each block start with a 16 byte header (the MAE), malloc
//  will return the address of the start of buffer (after the MAE) and NOT
//  the address of the block. 
//  That would be the address of the block + sizeof(MAE)
// 
//  The blocks are implemented as a linked list. A doubly linked list would 
//  increase the free() performances
// 
//  Right now, the OS supports 1 thread per process only. So there is no thread
//  safety to implement since only on thread can access the virtual memory at the
//  same time. Interupts will never use malloc to allocate mem on a user process
//  either. So this is perfectly safe for now. Eventually, it will be necessary to
//  lock the list on malloc/free. We should either lock the entire critical section,
//  or just implement a lock-free linked-list.
// 
// Physical Page management:
//  malloc() will not allocate physical pages as it reserves memory.
//  malloc only reserves virtual memory. When a task attempts to access a 
//  non-present page, a #PF will occur and the page will be allocated. When malloc
//  creates a new MAE, it creates it in free memory (at the end of the requested 
//  block). Chances are that this address resides on a page that is not allocated. 
//  When writting the MAE, a #PF will occur. This is the only time that malloc would 
//  trigger a physical memory allocation. Note that it is not malloc that allocated 
//  the memory but #PF. malloc has no idea what pages are free or not, it assumes
//  that all pages are available and that there is enough RAM to fulfill all 
//  requests. The pitfall is that, for this reason, malloc does not guarantee that 
//  memory is available when creating a buffer. The task could request a 10mb buffer 
//  to malloc but when trying to access it, a #PF might discover that no pages are 
//  available. So the process would crash.
// 
//  free() will only free virtual memory and not physical pages. Another mechanism 
//  needs to exist to collect those pages since they are not in use anymore.
// 
//  The key concept to understand is that malloc/free are virtual memory function 
//  and have no awareness of physical memory. Physical memory is handled by another 
//  layer so that malloc/free do not need to think about it.
// 
//  on process creation, a single physical page of memory is allocated for the heap. 
//  This page is created to store the first MAE.
// 
// malloc
//  Malloc's job is to reserve a memory block in the linked-list. It is not to 
//  allocate physical page and map them on virtual space. Physical pages will be 
//  allocated on the fly. Malloc is best described with pseudo-code:
//  ********
//  E = first MAE
// 
//  WHILE (true)
//      L = address of MAE
//      IF (E >= END_OF_HEAP)
//          exit(no_mem)
//      FI
//  
//      IF (E.inuse == 0)
//          BlockSize = E.next-L
//          IF (BlockSize) >= (requestedSize+16)
//               // this may trigger a page fault
//              TEMP = new MAE at location L+16+requestSize 
//              TEMP.next = E.next
//              TEMP.inuse = 0
//              E.next = ADDRESS(TEMP)
//              E.inuse = 1
//              exit(ADDRESS(E) + 16)
//          FI
//      FI
//  
//      E = E.next
//  ELIHW
//
//  ********
// 
// free(address)
//     VICTIM = (address-16)
//     NEXT = VICTIM.next
//     PREVIOUS = first MAE
// 
//     // This would not be needed if using doubly linked list
//     // Find previous
//     IF (PREVIOUS != VICTIM)
//         WHILE (true)
//             IF (PREVIOUS.next == VICTIM) break
//             IF (PREVIOUS >= END_OF_HEAP)
//                 exit(not_found)
//             FI
//             PREVIOUS = PREVIOUS.next
//         ELIHW
//     FI
// 
//     // previous and next could be free
//     // previous and next could be in use
//     // previous could be in use but not next
//     // previous could be free but not next
//     IF (PREVIOUS.inuse == 1 || PREVIOUS == VICTIM)
//         VICTIM.inuse = 0
//         IF (NEXT.inuse==0 && NEXT<HEAP_TOP)
//             // --> previous in use but not next
//             VICTIM.next = NEXT.next
//         ELSE
//             // --> previous and next are in use
//             //VICTIM.next = NEXT // redundant, it is already set like that.
//         FI
//     ELSE
// 
//         IF (NEXT.inuse==0 && NEXT<HEAP_TOP)
//             // --> previous and next ARE free
//             PREVIOUS.next = NEXT.next
//         ELSE
//             // --> previous is free but not next
//             PREVIOUS.next = NEXT
//         FI
// 
//     FI
//
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: malloc(rdi=size)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
malloc:
    push        %rsi
    push        %rdi
    push        %r8
    push        %r9
    push        %r10

    // add 16 to rdi since we want to allow for a header in the block
    // and then we round it up to the next 16byte boundary
    // so a requested buffer of 1, would turn into 32
    add         $(16+0xF),%rdi
    and         $~0x0F,%rdi

    movq        $PROCESS_HEAP_ADDRESS,%rsi
    mov         (%rsi),%rsi

    

    mov         $HEAP_TOP,%r10
1:  cmp         %r10,%rsi
    jae         malloc_no_mem        // if currentAddress >= HEAP_TOP

    test        $0x0F,%rsi
    jnz         heap_corrupted    

    // r8 =  control bits
    mov         (%rsi),%r8
    mov         8(%rsi),%rax
    shl         $4,%rax             // clear upper 4 bits
    shr         $4,%rax             // clear upper 4 bits

    cmp         %rax,%rsi
    jae         heap_corrupted

    // r9 = entry size (end-start)
    mov         %rax,%r9
    sub         %rsi,%r9

    // rsi = next entry, rax= current entry
    xchg        %rax,%rsi           

    // is that block free ?
    bt          $0,%r8
    jc          1b

    // is requested size(rdi) bigger than block size (r9). 
    // search again if it is.
    cmp         %r9,%rdi
    ja          1b

    // At this point, we found a candidate block

    // rdi = the new block
    add         %rax,%rdi

    // new.next = block.next
    mov         8(%rax),%r8
    mov         %r8,8(%rdi)

    // new.inuse = 0
    btr         $0,(%rdi)

    // block.next = new
    // block.inuse = 1
    mov         %rdi,8(%rax)
    bts         $0,(%rax)

    // return address = this block +16
    add         $0x10,%rax

    pop         %r10
    pop         %r9
    pop         %r8
    pop         %rdi
    pop         %rsi
    ret

malloc_no_mem:
    mov         $0xAAAAAAAA,%r15
    int         $3
    // TODO: should just return 0
heap_corrupted:
    mov         $0xDEADBEEF,%r15
    int         $3

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: init_heap(rdi=heap_phys_addr)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
init_heap:
    push        %rax
    movq        $0,(%rdi)
    movq        $(HEAP_TOP),%rax
    mov         %rax,8(%rdi)
    pop         %rax
    ret



