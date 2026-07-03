/*
 * startup.c - Reset handler, C-runtime bring-up, and the interrupt vector table.
 *
 * When the chip powers on, the Cortex-M4 loads the initial stack pointer from
 * address 0x0 and the reset vector from 0x4, then starts executing. Before any
 * C code can run correctly we must:
 *   1. copy initialised globals (.data) from flash into RAM,
 *   2. zero the .bss section,
 *   3. call main().
 * There is no C library doing this for us - that is what a startup file is.
 *
 * The vector table lives at the very start of flash (section .vectors, placed
 * first by the linker script). Each entry is a function pointer; unused ones
 * point at Default_Handler, which just spins so a stray interrupt is obvious
 * under a debugger.
 */
#include <stdint.h>

/* Symbols provided by the linker script (nrf52840.ld). */
extern uint32_t _sidata; /* .data init values, stored in flash */
extern uint32_t _sdata;  /* .data start in RAM                 */
extern uint32_t _edata;  /* .data end in RAM                   */
extern uint32_t _sbss;   /* .bss start in RAM                  */
extern uint32_t _ebss;   /* .bss end in RAM                    */
extern uint32_t _estack; /* top of stack (end of RAM)          */

int main(void);

/* ------------------------------------------------------------------------- */
/* Handlers. Every one is a weak alias to Default_Handler; a strong definition
 * elsewhere (e.g. RTC0_IRQHandler in rtc.c) transparently overrides it.       */

void Reset_Handler(void);
void Default_Handler(void);

#define WEAK_ALIAS __attribute__((weak, alias("Default_Handler")))

void NMI_Handler(void)        WEAK_ALIAS;
void HardFault_Handler(void)  WEAK_ALIAS;
void MemManage_Handler(void)  WEAK_ALIAS;
void BusFault_Handler(void)   WEAK_ALIAS;
void UsageFault_Handler(void) WEAK_ALIAS;
void SVC_Handler(void)        WEAK_ALIAS;
void DebugMon_Handler(void)   WEAK_ALIAS;
void PendSV_Handler(void)     WEAK_ALIAS;
void SysTick_Handler(void)    WEAK_ALIAS;

/* nRF52840 peripheral interrupts (subset named; all default unless overridden).
 * Index == IRQ number. RTC0 (11) is implemented in rtc.c. */
void POWER_CLOCK_IRQHandler(void) WEAK_ALIAS;
void RADIO_IRQHandler(void)       WEAK_ALIAS;
void RTC0_IRQHandler(void)        WEAK_ALIAS;
void RNG_IRQHandler(void)         WEAK_ALIAS;

/* ------------------------------------------------------------------------- */
/* The vector table. Placed at flash offset 0 by the .vectors section.        */

__attribute__((section(".vectors"), used))
void (*const vector_table[])(void) = {
    (void (*)(void))(&_estack), /* 0x00 initial stack pointer */
    Reset_Handler,              /* 0x04 reset                 */
    NMI_Handler,                /* 0x08                       */
    HardFault_Handler,          /* 0x0C                       */
    MemManage_Handler,          /* 0x10                       */
    BusFault_Handler,           /* 0x14                       */
    UsageFault_Handler,         /* 0x18                       */
    0, 0, 0, 0,                 /* 0x1C-0x28 reserved         */
    SVC_Handler,                /* 0x2C                       */
    DebugMon_Handler,           /* 0x30                       */
    0,                          /* 0x34 reserved              */
    PendSV_Handler,             /* 0x38                       */
    SysTick_Handler,            /* 0x3C                       */

    /* External interrupts, IRQ 0.. */
    POWER_CLOCK_IRQHandler,     /* 0  */
    RADIO_IRQHandler,           /* 1  */
    Default_Handler,            /* 2  UARTE0_UART0 */
    Default_Handler,            /* 3  */
    Default_Handler,            /* 4  */
    Default_Handler,            /* 5  NFCT */
    Default_Handler,            /* 6  GPIOTE */
    Default_Handler,            /* 7  SAADC */
    Default_Handler,            /* 8  TIMER0 */
    Default_Handler,            /* 9  TIMER1 */
    Default_Handler,            /* 10 TIMER2 */
    RTC0_IRQHandler,            /* 11 RTC0   */
    Default_Handler,            /* 12 TEMP   */
    RNG_IRQHandler,             /* 13 RNG    */
    Default_Handler,            /* 14 ECB    */
    Default_Handler,            /* 15 CCM_AAR */
    Default_Handler,            /* 16 WDT    */
    Default_Handler,            /* 17 RTC1   */
    Default_Handler,            /* 18 QDEC   */
    Default_Handler,            /* 19 COMP_LPCOMP */
    Default_Handler,            /* 20 SWI0_EGU0 */
    Default_Handler,            /* 21 SWI1_EGU1 */
    Default_Handler,            /* 22 SWI2_EGU2 */
    Default_Handler,            /* 23 SWI3_EGU3 */
    Default_Handler,            /* 24 SWI4_EGU4 */
    Default_Handler,            /* 25 SWI5_EGU5 */
    Default_Handler,            /* 26 TIMER3 */
    Default_Handler,            /* 27 TIMER4 */
    Default_Handler,            /* 28 PWM0   */
    Default_Handler,            /* 29 PDM    */
};

/* ------------------------------------------------------------------------- */

void Reset_Handler(void)
{
    /* Copy .data (initialised globals) from its flash load address into RAM. */
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* Zero .bss (uninitialised globals). */
    for (dst = &_sbss; dst < &_ebss; dst++) {
        *dst = 0;
    }

    (void)main();

    /* main() should never return; if it does, hang deterministically. */
    for (;;) {
    }
}

void Default_Handler(void)
{
    for (;;) {
        /* Unexpected interrupt - park here so a debugger can catch it. */
    }
}
