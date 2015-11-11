ENTRY(main)
SECTIONS {
    . = (0x08000200);
    .text : {
        * (.text);
    }
    .rodata ALIGN(0x1000) : {
        * (.rodata);
    }
    .data ALIGN(0x1000) : {
        * (.data);
    }
    .bss ALIGN(0x1000) : {
        * (.bss);
    }
    /DISCARD/ :
   {
        *(.*)
   }
}
