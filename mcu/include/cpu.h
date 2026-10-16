
#ifndef CPU_H
    #define CPU_H


static inline void cpu_nop(void)
{
    __asm volatile ("nop");
}


#endif
