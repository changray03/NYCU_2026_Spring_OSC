.section ".text.boot"
.global _start
_start:
    mv s0, a0          // s0 = hartid
    mv s1, a1          // s1 = dtb_ptr

    // Set up the stack pointer
    la sp, _end
    
    la t0, __bss_start
    la t1, __bss_stop

// Clear the .bss section
bss_clear:
    bge t0, t1, bss_done
    sb zero, 0(t0)
    addi t0, t0, 1
    j bss_clear

bss_done:
    mv a0, s0
    mv a1, s1
    tail start_kernel
