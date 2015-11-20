.global create_memory_pool
.global reserve_object
.global release_object
.global destroy_memory_pool

//WARNING: The memory pool was not tested. This pool will not
//         work accross processes. It should be modified to use
//         physical memory instead (maybe pages directly instead of malloc)
//         The memory must be continguous in phys mem because
//         if an object span accross two pages, it need to be identical
//         on every process

// Memory pool:
//  A pool is created with malloc with size count*(((object_size+15)&~0xF)+16)
//  Each object will be prefixed witha 16byte header and each slot begins
//  on a 16byte boundary. The header's bit0 indicates if object is free or not
//
// Limitations:
//  Pool does not grow on demand. It is of fixed size
//  Pool is a regular list, not a linked-list


///////////////////////////////////////////////////////////////////////////////////// 
///////////////////////////////////////////////////////////////////////////////////// 
// Function: create_memory_pool(rdi=object_size,rsi=count) 
// Return: pool address                                                                                    
///////////////////////////////////////////////////////////////////////////////////// 
///////////////////////////////////////////////////////////////////////////////////// 
create_memory_pool:
    push        %rdx
    push        %r8

    // total mem = 
    //      16bytes for pool header
    //      + count*(((object_size+15)&~0xF)+16)
    push        %rsi
    push        %rdi
    mov         %rdi,%rax
    add         $(16+15),%rsi
    and         $~0x0F,%rsi
    mov         %rsi,%r8
    mul         %rsi
    add         $16,%rax        
    mov         %rax,%rdi   
    call        malloc

    // clear the pool
    push        %rax
    push        %rcx
    mov         %rdi,%rcx
    mov         %rax,%rdi
    shr         $3,%rcx
    xor         %rax,%rax
    rep stosq
    pop         %rcx
    pop         %rax
    pop         %rdi
    pop         %rsi
    mov         %r8,(%rax)     // save obj size in pool header
    mov         %rsi,8(%rax)    // save obj count in pool header
    pop         %r8
    pop         %rdx
    ret

///////////////////////////////////////////////////////////////////////////////////// 
///////////////////////////////////////////////////////////////////////////////////// 
// Function: reserve_object(rdi=pool_address)
// Return:  a pointer to an object or 0 is no memory available.
//          This function guarantees that the object will be aligned on a 
//          16byte boundary.
///////////////////////////////////////////////////////////////////////////////////// 
///////////////////////////////////////////////////////////////////////////////////// 
reserve_object:                                                           
    push        %rcx
    push        %rdi
    mov         (%rdi),%rax     // rax = object size
    add         $16,%rdi        // add the object header size
    mov         8(%rdi),%rcx    // rcx = object count

    add         $16,%rdi        // skip pool header
    
1:  lock bts    $0,(%rdi)
    jnc         3f
    add         %rax,%rdi
    loop        1b
    xor         %rax,%rax
2:  pop         %rdi
    pop         %rcx
    ret
3:  mov         %rdi,%rax
    add         $16,%rax
    jmp         2b


///////////////////////////////////////////////////////////////////////////////////// 
///////////////////////////////////////////////////////////////////////////////////// 
// Function: release_object(rdi=pool_address, rsi=pointer_to_object)
//                                                                                    
///////////////////////////////////////////////////////////////////////////////////// 
///////////////////////////////////////////////////////////////////////////////////// 
release_object:
    btr         $0,-16(%rsi)                                                 
    ret

///////////////////////////////////////////////////////////////////////////////////// 
///////////////////////////////////////////////////////////////////////////////////// 
// Function: destroy_memory_pool(rdi=pool_address)
//                                                                                    
///////////////////////////////////////////////////////////////////////////////////// 
///////////////////////////////////////////////////////////////////////////////////// 
destroy_memory_pool:
    call        free
    ret
