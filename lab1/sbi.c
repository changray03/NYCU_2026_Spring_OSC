struct sbiret {
	long error;
	long value;
};

struct sbiret sbi_ecall(int ext, int fid,
                        unsigned long arg0,
                        unsigned long arg1,
                        unsigned long arg2,
                        unsigned long arg3,
                        unsigned long arg4,
                        unsigned long arg5)
{
    register unsigned long a0 asm("a0") = (unsigned long)arg0;  // Reserved word register let the compiler to use register a0 but not memory to store the value of arg0
    register unsigned long a1 asm("a1") = (unsigned long)arg1;
    register unsigned long a2 asm("a2") = (unsigned long)arg2;
    register unsigned long a3 asm("a3") = (unsigned long)arg3;
    register unsigned long a4 asm("a4") = (unsigned long)arg4;
    register unsigned long a5 asm("a5") = (unsigned long)arg5;
    register unsigned long a6 asm("a6") = (unsigned long)fid;
    register unsigned long a7 asm("a7") = (unsigned long)ext;

    asm volatile ("ecall"
                  : "+r" (a0), "+r" (a1)
                  : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
                  : "memory");

    struct sbiret ret = { .error = a0, .value = a1 };
    return ret;
}

struct sbiret sbi_get_spec_version(){
    return sbi_ecall(0x10, 0, 0, 0, 0, 0, 0, 0);
}

struct sbiret sbi_get_impl_id(){
    return sbi_ecall(0x10, 1, 0, 0, 0, 0, 0, 0);
}

struct sbiret sbi_get_impl_version(){
    return sbi_ecall(0x10, 2, 0, 0, 0, 0, 0, 0);
}