#ifndef TRAP_H
#define TRAP_H

struct pt_regs {
    unsigned long ra;      // 8 * 0: Return address
    unsigned long sp;      // 8 * 1: 原本的 user sp (從 sscratch 讀取)
    unsigned long gp;      // 8 * 2: Global pointer
    unsigned long tp;      // 8 * 3: Thread pointer
    unsigned long t0;      // 8 * 4: Temporary 0
    unsigned long t1;      // 8 * 5: Temporary 1
    unsigned long t2;      // 8 * 6: Temporary 2
    unsigned long s0;      // 8 * 7: Saved register 0 / frame pointer
    unsigned long s1;      // 8 * 8: Saved register 1
    unsigned long a0;      // 8 * 9: Function argument 0 / return value
    unsigned long a1;      // 8 * 10: Function argument 1
    unsigned long a2;      // 8 * 11: ...
    unsigned long a3;      // 8 * 12:
    unsigned long a4;      // 8 * 13:
    unsigned long a5;      // 8 * 14:
    unsigned long a6;      // 8 * 15:
    unsigned long a7;      // 8 * 16:
    unsigned long s2;      // 8 * 17: Saved register 2
    unsigned long s3;      // 8 * 18: ...
    unsigned long s4;      // 8 * 19:
    unsigned long s5;      // 8 * 20:
    unsigned long s6;      // 8 * 21:
    unsigned long s7;      // 8 * 22:
    unsigned long s8;      // 8 * 23:
    unsigned long s9;      // 8 * 24:
    unsigned long s10;     // 8 * 25:
    unsigned long s11;     // 8 * 26:
    unsigned long t3;      // 8 * 27: Temporary 3
    unsigned long t4;      // 8 * 28: Temporary 4
    unsigned long t5;      // 8 * 29: Temporary 5
    unsigned long t6;      // 8 * 30: Temporary 6
    unsigned long epc;     // 8 * 31: Exception Program Counter
    unsigned long sstatus; // 8 * 32: Supervisor Status Register
    unsigned long scause;  // 8 * 33: Supervisor Cause Register
    unsigned long stval;   // 8 * 34: Supervisor Trap Value Register
};
void do_trap(struct pt_regs* regs);

#endif