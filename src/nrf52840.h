/*
 * nrf52840.h - Hand-written register map for the Nordic nRF52840.
 *
 * There is no CMSIS, no nrfx, no SoftDevice and no Zephyr here. Every symbol
 * below is a raw memory-mapped register whose address and offset are taken
 * directly from the "nRF52840 Product Specification" (the offsets are quoted in
 * the comments so you can cross-check against the datasheet as you read).
 *
 * Why do it this way? Because the whole point of this project is to *see* the
 * hardware. An SDK would hide FICR, RADIO, PCNF0/1, whitening and the CRC unit
 * behind tidy function calls. Here you configure them yourself, one 32-bit
 * store at a time.
 *
 * Convention used throughout:
 *   - A register is exposed as an lvalue macro that dereferences a
 *     `volatile uint32_t *`. `volatile` is mandatory: it tells the compiler the
 *     memory can change underneath it (hardware events) and must never be cached
 *     or reordered away.
 *   - TASKS_* are write-1-to-trigger registers (fire an action).
 *   - EVENTS_* are set-by-hardware flags you poll and then clear by writing 0.
 *
 * Datasheet: https://docs.nordicsemi.com/bundle/ps_nrf52840/page/keyfeatures_html5.html
 */
#ifndef NRF52840_H
#define NRF52840_H

#include <stdint.h>

/* Generic accessor: treat `addr` as a 32-bit hardware register. */
#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

/* ------------------------------------------------------------------------- */
/* FICR - Factory Information Configuration Registers (read-only, base 0x10000000)
 *
 * Programmed at the factory, cannot be erased. DEVICEADDR is the per-chip
 * random address the Bluetooth stack would normally use.
 */
#define FICR_BASE            0x10000000UL
#define FICR_DEVICEID0       REG32(FICR_BASE + 0x060) /* Device id, word 0        */
#define FICR_DEVICEID1       REG32(FICR_BASE + 0x064) /* Device id, word 1        */
#define FICR_DEVICEADDRTYPE  REG32(FICR_BASE + 0x0A0) /* bit0: 0=public,1=random  */
#define FICR_DEVICEADDR0     REG32(FICR_BASE + 0x0A4) /* Address bytes 0..3 (LE)  */
#define FICR_DEVICEADDR1     REG32(FICR_BASE + 0x0A8) /* Address bytes 4..5 (LE)  */

/* ------------------------------------------------------------------------- */
/* CLOCK - base 0x40000000. Starts the high- and low-frequency clock sources. */
#define CLOCK_BASE                 0x40000000UL
#define CLOCK_TASKS_HFCLKSTART     REG32(CLOCK_BASE + 0x000)
#define CLOCK_TASKS_HFCLKSTOP      REG32(CLOCK_BASE + 0x004)
#define CLOCK_TASKS_LFCLKSTART     REG32(CLOCK_BASE + 0x008)
#define CLOCK_TASKS_LFCLKSTOP      REG32(CLOCK_BASE + 0x00C)
#define CLOCK_EVENTS_HFCLKSTARTED  REG32(CLOCK_BASE + 0x100)
#define CLOCK_EVENTS_LFCLKSTARTED  REG32(CLOCK_BASE + 0x104)
#define CLOCK_HFCLKSTAT            REG32(CLOCK_BASE + 0x40C)
#define CLOCK_LFCLKSTAT            REG32(CLOCK_BASE + 0x418)
#define CLOCK_LFCLKSRC             REG32(CLOCK_BASE + 0x518) /* 0=RC 1=XTAL 2=Synth */
#define CLOCK_LFCLKSRC_XTAL        1UL

/* ------------------------------------------------------------------------- */
/* RADIO - base 0x40001000. The 2.4 GHz transceiver. This is the star of the
 * show: we drive it manually to emit BLE advertising packets.
 */
#define RADIO_BASE               0x40001000UL
#define RADIO_TASKS_TXEN         REG32(RADIO_BASE + 0x000) /* enable + ramp up TX */
#define RADIO_TASKS_RXEN         REG32(RADIO_BASE + 0x004)
#define RADIO_TASKS_START        REG32(RADIO_BASE + 0x008)
#define RADIO_TASKS_STOP         REG32(RADIO_BASE + 0x00C)
#define RADIO_TASKS_DISABLE      REG32(RADIO_BASE + 0x010)
#define RADIO_EVENTS_READY       REG32(RADIO_BASE + 0x100) /* ramp-up complete    */
#define RADIO_EVENTS_ADDRESS     REG32(RADIO_BASE + 0x104)
#define RADIO_EVENTS_PAYLOAD     REG32(RADIO_BASE + 0x108)
#define RADIO_EVENTS_END         REG32(RADIO_BASE + 0x10C) /* packet done         */
#define RADIO_EVENTS_DISABLED    REG32(RADIO_BASE + 0x110) /* radio idle again    */
#define RADIO_SHORTS             REG32(RADIO_BASE + 0x200) /* auto task<-event    */
#define RADIO_INTENSET           REG32(RADIO_BASE + 0x304)
#define RADIO_INTENCLR           REG32(RADIO_BASE + 0x308)
#define RADIO_CRCSTATUS          REG32(RADIO_BASE + 0x400)
#define RADIO_RSSISAMPLE         REG32(RADIO_BASE + 0x548)
#define RADIO_STATE              REG32(RADIO_BASE + 0x550)
#define RADIO_PACKETPTR          REG32(RADIO_BASE + 0x504) /* EasyDMA src, RAM!   */
#define RADIO_FREQUENCY          REG32(RADIO_BASE + 0x508) /* MHz above 2400      */
#define RADIO_TXPOWER            REG32(RADIO_BASE + 0x50C)
#define RADIO_MODE               REG32(RADIO_BASE + 0x510)
#define RADIO_PCNF0              REG32(RADIO_BASE + 0x514) /* on-air packet format */
#define RADIO_PCNF1              REG32(RADIO_BASE + 0x518)
#define RADIO_BASE0              REG32(RADIO_BASE + 0x51C) /* access address low  */
#define RADIO_BASE1              REG32(RADIO_BASE + 0x520)
#define RADIO_PREFIX0            REG32(RADIO_BASE + 0x524) /* access address high */
#define RADIO_PREFIX1            REG32(RADIO_BASE + 0x528)
#define RADIO_TXADDRESS          REG32(RADIO_BASE + 0x52C) /* logical addr to TX  */
#define RADIO_RXADDRESSES        REG32(RADIO_BASE + 0x530)
#define RADIO_CRCCNF             REG32(RADIO_BASE + 0x534)
#define RADIO_CRCPOLY            REG32(RADIO_BASE + 0x538)
#define RADIO_CRCINIT            REG32(RADIO_BASE + 0x53C)
#define RADIO_DATAWHITEIV        REG32(RADIO_BASE + 0x554) /* whitening seed=chan */
#define RADIO_MODECNF0           REG32(RADIO_BASE + 0x650) /* ramp-up mode        */
#define RADIO_POWER              REG32(RADIO_BASE + 0xFFC)

/* RADIO.MODE values */
#define RADIO_MODE_BLE_1MBIT     3UL

/* RADIO.SHORTS bits: chain events straight to tasks with zero CPU latency. */
#define RADIO_SHORTS_READY_START (1UL << 0) /* READY  -> START   */
#define RADIO_SHORTS_END_DISABLE (1UL << 1) /* END    -> DISABLE */

/* RADIO.PCNF0 field positions (see PS "PCNF0"). */
#define RADIO_PCNF0_LFLEN_Pos    0   /* length-field size in bits            */
#define RADIO_PCNF0_S0LEN_Pos    8   /* S0 field size in bytes               */
#define RADIO_PCNF0_S1LEN_Pos    16  /* S1 field size in bits                */
#define RADIO_PCNF0_PLEN_Pos     24  /* preamble length (0 = 8 bit / 1 Mbit) */

/* RADIO.PCNF1 field positions. */
#define RADIO_PCNF1_MAXLEN_Pos   0   /* max payload length                   */
#define RADIO_PCNF1_STATLEN_Pos  8   /* extra "static" payload bytes         */
#define RADIO_PCNF1_BALEN_Pos    16  /* base address length in bytes         */
#define RADIO_PCNF1_ENDIAN_Pos   24  /* 0 = little endian on air             */
#define RADIO_PCNF1_WHITEEN_Pos  25  /* 1 = enable data whitening            */

/* RADIO.CRCCNF */
#define RADIO_CRCCNF_LEN_Pos       0
#define RADIO_CRCCNF_SKIPADDR_Pos  8 /* 1 = do not include address in CRC    */

/* RADIO.MODECNF0 */
#define RADIO_MODECNF0_RU_FAST   (1UL << 0) /* fast ramp-up (~40us vs ~140us) */

/* A handful of TXPOWER settings (signed dBm mapped to the register's u8). */
#define RADIO_TXPOWER_POS8DBM    0x08UL
#define RADIO_TXPOWER_POS4DBM    0x04UL
#define RADIO_TXPOWER_0DBM       0x00UL
#define RADIO_TXPOWER_NEG4DBM    0xFCUL
#define RADIO_TXPOWER_NEG8DBM    0xF8UL
#define RADIO_TXPOWER_NEG16DBM   0xF0UL
#define RADIO_TXPOWER_NEG40DBM   0xD8UL

/* ------------------------------------------------------------------------- */
/* RTC0 - base 0x4000B000. 24-bit counter clocked from the 32.768 kHz LFCLK.
 * We use it as a low-power wakeup timer between advertising bursts.
 */
#define RTC0_BASE                0x4000B000UL
#define RTC0_TASKS_START         REG32(RTC0_BASE + 0x000)
#define RTC0_TASKS_STOP          REG32(RTC0_BASE + 0x004)
#define RTC0_TASKS_CLEAR         REG32(RTC0_BASE + 0x008)
#define RTC0_EVENTS_TICK         REG32(RTC0_BASE + 0x100)
#define RTC0_EVENTS_COMPARE0     REG32(RTC0_BASE + 0x140)
#define RTC0_INTENSET            REG32(RTC0_BASE + 0x304)
#define RTC0_INTENCLR            REG32(RTC0_BASE + 0x308)
#define RTC0_EVTENSET            REG32(RTC0_BASE + 0x344)
#define RTC0_COUNTER             REG32(RTC0_BASE + 0x504)
#define RTC0_PRESCALER           REG32(RTC0_BASE + 0x508)
#define RTC0_CC0                 REG32(RTC0_BASE + 0x540)
#define RTC0_INTEN_COMPARE0      (1UL << 16)
#define RTC0_EVTEN_COMPARE0      (1UL << 16)

/* ------------------------------------------------------------------------- */
/* RNG - base 0x4000D000. Hardware entropy source; we use it for the BLE
 * mandated random advertising delay (jitter).
 */
#define RNG_BASE                 0x4000D000UL
#define RNG_TASKS_START          REG32(RNG_BASE + 0x000)
#define RNG_TASKS_STOP           REG32(RNG_BASE + 0x004)
#define RNG_EVENTS_VALRDY        REG32(RNG_BASE + 0x100)
#define RNG_CONFIG               REG32(RNG_BASE + 0x504) /* bit0 DERCEN=debias */
#define RNG_VALUE                REG32(RNG_BASE + 0x508)
#define RNG_CONFIG_DERCEN        (1UL << 0)

/* ------------------------------------------------------------------------- */
/* GPIO port 0 - base 0x50000000. The DK's four user LEDs live here. */
#define GPIO_P0_BASE             0x50000000UL
#define GPIO_P0_OUT              REG32(GPIO_P0_BASE + 0x504)
#define GPIO_P0_OUTSET           REG32(GPIO_P0_BASE + 0x508)
#define GPIO_P0_OUTCLR           REG32(GPIO_P0_BASE + 0x50C)
#define GPIO_P0_IN               REG32(GPIO_P0_BASE + 0x510)
#define GPIO_P0_DIRSET           REG32(GPIO_P0_BASE + 0x518)
#define GPIO_P0_DIRCLR           REG32(GPIO_P0_BASE + 0x51C)
#define GPIO_P0_PIN_CNF(n)       REG32(GPIO_P0_BASE + 0x700 + 4UL * (n))
/* PIN_CNF bits */
#define GPIO_PIN_CNF_INPUT       (0UL << 0) /* connect input buffer          */
#define GPIO_PIN_CNF_PULLUP      (3UL << 2)

/* ------------------------------------------------------------------------- */
/* Cortex-M NVIC (interrupt controller) - base 0xE000E100. Just enough to
 * enable the one IRQ we care about (RTC0).
 */
#define NVIC_ISER0               REG32(0xE000E100UL)
#define NVIC_ICER0               REG32(0xE000E180UL)

/* IRQ numbers (peripheral ID == IRQ number on nRF52). */
#define IRQ_RADIO   1
#define IRQ_RTC0    11
#define IRQ_RNG     13

/* ------------------------------------------------------------------------- */
/* Tiny helpers used across the firmware. */

/* Data/instruction sync barriers - order memory accesses around hardware. */
static inline void nrf_dsb(void) { __asm volatile("dsb 0xF" ::: "memory"); }
static inline void nrf_isb(void) { __asm volatile("isb 0xF" ::: "memory"); }

/* Sleep the core until an event/interrupt wakes it (low power). */
static inline void nrf_wfe(void) { __asm volatile("wfe"); }
static inline void nrf_sev(void) { __asm volatile("sev"); }

static inline void nvic_enable_irq(uint32_t irq) { NVIC_ISER0 = (1UL << irq); }

#endif /* NRF52840_H */
