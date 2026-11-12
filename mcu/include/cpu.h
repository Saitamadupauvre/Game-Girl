
#ifndef CPU_H
    #define CPU_H


static inline void cpu_nop(void)
{
    __asm volatile ("nop");
}

// Waits until every earlier memory access, such as an NVIC write, has completed
static inline void cpu_dsb(void)
{
    __asm volatile ("dsb 0xF" ::: "memory");
}

// Flushes the pipeline so the following instructions see the effect of the barrier
static inline void cpu_isb(void)
{
    __asm volatile ("isb 0xF" ::: "memory");
}


#endif
