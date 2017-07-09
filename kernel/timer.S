.global apic_timer_handler

apic_timer_handler:
    call        video_handler
    jmp         schedulerHandler  // will do the iretq
