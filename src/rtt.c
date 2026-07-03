/*
 * rtt.c - see rtt.h.
 *
 * The control block must be a byte-for-byte match for what a host RTT reader
 * expects, because the host finds it by scanning target RAM for the 16-byte
 * magic id "SEGGER RTT". Layout (matches the SEGGER spec):
 *
 *   struct {
 *     char     acID[16];            // "SEGGER RTT\0..."
 *     int32_t  MaxNumUpBuffers;     // 1
 *     int32_t  MaxNumDownBuffers;   // 1
 *     Buffer   aUp[1];              // target -> host
 *     Buffer   aDown[1];            // host -> target
 *   }
 *   struct Buffer {
 *     const char *sName;
 *     char       *pBuffer;
 *     uint32_t    SizeOfBuffer;
 *     uint32_t    WrOff;            // up: written by target
 *     uint32_t    RdOff;            // up: written by host
 *     uint32_t    Flags;
 *   }
 */
#include "rtt.h"
#include <stdarg.h>
#include "nrf52840.h"

#define RTT_UP_BUFFER_SIZE   1024
#define RTT_DOWN_BUFFER_SIZE 16
#define RTT_MODE_NO_BLOCK_TRIM 1u /* drop what does not fit, never stall */

typedef struct {
    const char *sName;
    char *pBuffer;
    uint32_t SizeOfBuffer;
    volatile uint32_t WrOff;
    volatile uint32_t RdOff;
    uint32_t Flags;
} rtt_buffer_t;

typedef struct {
    char acID[16];
    int32_t MaxNumUpBuffers;
    int32_t MaxNumDownBuffers;
    rtt_buffer_t aUp[1];
    rtt_buffer_t aDown[1];
} rtt_control_block_t;

static char s_up_buf[RTT_UP_BUFFER_SIZE];
static char s_down_buf[RTT_DOWN_BUFFER_SIZE];

/* The symbol name _SEGGER_RTT is conventional; some tools look for it directly.
 * We deliberately leave acID zeroed here and fill it in rtt_init() so the magic
 * string exists at exactly one place in RAM once, and never sits pre-formed in
 * flash where a scanner might match the wrong copy. */
rtt_control_block_t _SEGGER_RTT;

void rtt_init(void)
{
    _SEGGER_RTT.MaxNumUpBuffers = 1;
    _SEGGER_RTT.MaxNumDownBuffers = 1;

    _SEGGER_RTT.aUp[0].sName = "Terminal";
    _SEGGER_RTT.aUp[0].pBuffer = s_up_buf;
    _SEGGER_RTT.aUp[0].SizeOfBuffer = RTT_UP_BUFFER_SIZE;
    _SEGGER_RTT.aUp[0].WrOff = 0;
    _SEGGER_RTT.aUp[0].RdOff = 0;
    _SEGGER_RTT.aUp[0].Flags = RTT_MODE_NO_BLOCK_TRIM;

    _SEGGER_RTT.aDown[0].sName = "Terminal";
    _SEGGER_RTT.aDown[0].pBuffer = s_down_buf;
    _SEGGER_RTT.aDown[0].SizeOfBuffer = RTT_DOWN_BUFFER_SIZE;
    _SEGGER_RTT.aDown[0].WrOff = 0;
    _SEGGER_RTT.aDown[0].RdOff = 0;
    _SEGGER_RTT.aDown[0].Flags = RTT_MODE_NO_BLOCK_TRIM;

    /* Write the magic id last, with a barrier, so the block is fully formed
     * before a host could recognise it. */
    static const char id[] = "SEGGER RTT";
    for (uint32_t i = 0; i < sizeof(id); i++) {
        _SEGGER_RTT.acID[i] = id[i];
    }
    nrf_dsb();
}

static void rtt_write(const char *data, uint32_t len)
{
    rtt_buffer_t *up = &_SEGGER_RTT.aUp[0];
    uint32_t wr = up->WrOff;
    uint32_t size = up->SizeOfBuffer;

    while (len--) {
        uint32_t next = wr + 1;
        if (next == size) {
            next = 0;
        }
        if (next == up->RdOff) {
            break; /* buffer full: trim the rest (non-blocking) */
        }
        s_up_buf[wr] = *data++;
        wr = next;
    }

    nrf_dsb();
    up->WrOff = wr; /* publish new write offset for the host */
}

void rtt_write_str(const char *s)
{
    uint32_t len = 0;
    while (s[len]) {
        len++;
    }
    rtt_write(s, len);
}

/* --- tiny formatter ------------------------------------------------------ */

static void emit_char(char c) { rtt_write(&c, 1); }

static void emit_udec(uint32_t v)
{
    char tmp[10];
    int n = 0;
    if (v == 0) {
        emit_char('0');
        return;
    }
    while (v) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (n--) {
        emit_char(tmp[n]);
    }
}

static void emit_hex(uint32_t v, int min_digits)
{
    static const char hexd[] = "0123456789abcdef";
    char tmp[8];
    int n = 0;
    if (v == 0) {
        tmp[n++] = '0';
    }
    while (v) {
        tmp[n++] = hexd[v & 0xf];
        v >>= 4;
    }
    while (n < min_digits) {
        tmp[n++] = '0';
    }
    while (n--) {
        emit_char(tmp[n]);
    }
}

void rtt_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            emit_char(*p);
            continue;
        }
        p++;

        /* optional zero-padded width for hex, e.g. %02x .. %08x */
        int width = 0;
        if (*p == '0') {
            p++;
            while (*p >= '1' && *p <= '9') {
                width = width * 10 + (*p - '0');
                p++;
            }
        }

        switch (*p) {
        case 's': rtt_write_str(va_arg(ap, const char *)); break;
        case 'c': emit_char((char)va_arg(ap, int)); break;
        case 'u': emit_udec(va_arg(ap, uint32_t)); break;
        case 'd': {
            int32_t v = va_arg(ap, int32_t);
            if (v < 0) {
                emit_char('-');
                emit_udec((uint32_t)(-v));
            } else {
                emit_udec((uint32_t)v);
            }
            break;
        }
        case 'x': emit_hex(va_arg(ap, uint32_t), width); break;
        case '%': emit_char('%'); break;
        default:
            emit_char('%');
            emit_char(*p);
            break;
        }
    }

    va_end(ap);
}
