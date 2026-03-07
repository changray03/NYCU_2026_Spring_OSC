.section ".text.boot"
.global _start
_start:
    // TODO: Clear the .bss section
    la t0, __bss_start  // t0 設為 BSS 起點
    la t1, __bss_stop   // t1 = BSS 終點
zerod:
    bge t0, t1, zerod_done  // 如果 t0 >= t1，跳出迴圈
    sb zero, 0(t0)          // 將 0 寫入 t0 指向的位址
    addi t0, t0, 1          // t0 往後移 1 byte
    j zerod                 // 繼續下一圈
zerod_done:
    // TODO: Set up the stack pointer
    la sp, _end
    tail start_kernel
