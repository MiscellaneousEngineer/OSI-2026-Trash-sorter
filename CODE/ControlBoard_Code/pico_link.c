/*
 * pico_link.c - Jetson -> Pico 2 UART line reader (no checksum) plus
 *               hardware multicore FIFO for handing parsed records to
 *               core0.
 *
 * Designed for the reader to run alone on core1.  No printing.
 */

#include "pico_link.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* Protocol max line is ~30 chars. 64 = comfortable safety margin. */
#define LINE_BUF_SIZE  64

/* -- Reader state (core1 only) --------------------------------------- */
static uart_inst_t *s_uart = NULL;
static char         s_buf[LINE_BUF_SIZE];
static size_t       s_len = 0;

/* -- Cross-core liveness (core1 writes, core0 reads) ----------------- */
/* Aligned 32-bit word: read/write is atomic on Cortex-M33. */
static volatile uint32_t s_last_rx_ms = 0;


/* ========================================================================
 * Reader: init
 * ======================================================================== */
void pico_link_init(uart_inst_t *uart, uint baud, uint tx_pin, uint rx_pin)
{
    s_uart = uart;
    s_len  = 0;

    uart_init(uart, baud);
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);

    uart_set_format(uart, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart, true);
    uart_set_hw_flow(uart, false, false);
}


/* ========================================================================
 * Reader: helpers
 * ======================================================================== */

/* Pull next comma-delimited token, NUL-terminating it in place.
 * Advances *p past the comma; sets *p = NULL after the last token. */
static char *next_field(char **p)
{
    if (*p == NULL) return NULL;
    char *start = *p;
    char *comma = strchr(start, ',');
    if (comma) {
        *comma = '\0';
        *p = comma + 1;
    } else {
        *p = NULL;
    }
    return start;
}

/* Map a wire-protocol material number (1..6) to the bin_id_t enum.
 * Out-of-range values (0, negative, > 6) become BIN_UNKNOWN, which
 * the parser treats as a malformed line. */
static bin_id_t material_num_to_bin(long n)
{
    if (n >= BIN_PLASTIC && n <= BIN_OTHER) {
        return (bin_id_t)n;
    }
    return BIN_UNKNOWN;
}

/* Parse one (NUL-terminated, no '\n') line into *out.
 * Returns true on success; *out untouched on failure. */
static bool parse_line(char *line, detection_t *out)
{
    char *p = line;
    char *tok;
    char *endp;

    /* Field 1: record tag (must be a single 'M') */
    tok = next_field(&p);
    if (!tok || tok[0] == '\0' || tok[1] != '\0') return false;
    if (tok[0] != 'M') return false;
    msg_type_t type = MSG_DETECTION;

    /* Field 2: material number (1..6) */
    tok = next_field(&p);
    if (!tok || tok[0] == '\0') return false;
    long mat = strtol(tok, &endp, 10);
    if (*endp != '\0') return false;
    bin_id_t bin = material_num_to_bin(mat);
    if (bin == BIN_UNKNOWN) return false;

    /* Field 3: cx (signed integer) */
    tok = next_field(&p);
    if (!tok || tok[0] == '\0') return false;
    long cx = strtol(tok, &endp, 10);
    if (*endp != '\0') return false;

    /* Field 4: cy (signed integer) */
    tok = next_field(&p);
    if (!tok || tok[0] == '\0') return false;
    long cy = strtol(tok, &endp, 10);
    if (*endp != '\0') return false;

    /* Field 5: conf (float) */
    tok = next_field(&p);
    if (!tok || tok[0] == '\0') return false;
    float conf = strtof(tok, &endp);
    if (*endp != '\0') return false;

    /* Field 6: cam_id (unsigned integer) */
    tok = next_field(&p);
    if (!tok || tok[0] == '\0') return false;
    long cam = strtol(tok, &endp, 10);
    if (*endp != '\0') return false;

    /* No trailing fields */
    if (p != NULL) return false;

    /* Range checks */
    if (cx   < INT16_MIN || cx   > INT16_MAX) return false;
    if (cy   < INT16_MIN || cy   > INT16_MAX) return false;
    if (cam  < 0         || cam  > 255)       return false;
    if (conf < 0.0f      || conf > 1.0f)      return false;

    out->type   = type;
    out->bin    = bin;
    out->cx     = (int16_t)cx;
    out->cy     = (int16_t)cy;
    out->conf   = conf;
    out->cam_id = (uint8_t)cam;
    return true;
}


/* ========================================================================
 * Reader: public read entry point
 * ======================================================================== */
bool pico_link_read_blocking(detection_t *out)
{
    for (;;) {
        char c = uart_getc(s_uart);   /* blocks until a byte is ready */

        if (c == '\r') continue;      /* tolerate CRLF */

        if (c == '\n') {
            if (s_len == 0) continue; /* ignore empty lines */
            s_buf[s_len] = '\0';
            s_len = 0;
            if (parse_line(s_buf, out)) {
                /* Stamp liveness on success only; garbage lines don't
                 * count as "received OK". */
                s_last_rx_ms = to_ms_since_boot(get_absolute_time());
                return true;
            }
            return false;
        }

        /* Overrun: drop the rest of this line, resync on next '\n' */
        if (s_len >= LINE_BUF_SIZE - 1) {
            s_len = 0;
            char d;
            do { d = uart_getc(s_uart); } while (d != '\n');
            continue;
        }

        s_buf[s_len++] = c;
    }
}


/* ========================================================================
 * Liveness getter
 * ======================================================================== */
uint32_t pico_link_last_rx_ms(void)
{
    return s_last_rx_ms;
}


/* ========================================================================
 * Hardware multicore FIFO transport
 *
 * 3 words per detection:
 *     w0 = bin            (low 8 bits)
 *     w1 = cx (lo16) | cy (hi16)
 *     w2 = conf as raw float bits
 * ======================================================================== */

static inline uint32_t float_to_bits(float f)
{
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}
static inline float bits_to_float(uint32_t bits)
{
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

void pico_link_fifo_push(const detection_t *det)
{
    uint32_t w0 = (uint32_t)det->bin & 0xFFu;
    uint32_t w1 = ((uint32_t)(uint16_t)det->cx)
                | ((uint32_t)(uint16_t)det->cy << 16);
    uint32_t w2 = float_to_bits(det->conf);

    /* push_blocking only stalls when the 8-entry FIFO is full; the
     * three calls go through back-to-back so framing is preserved. */
    multicore_fifo_push_blocking(w0);
    multicore_fifo_push_blocking(w1);
    multicore_fifo_push_blocking(w2);
}

static void fifo_unpack(uint32_t w0, uint32_t w1, uint32_t w2,
                        detection_t *out)
{
    out->type   = MSG_DETECTION;
    out->bin    = (bin_id_t)(w0 & 0xFFu);
    out->cx     = (int16_t)(uint16_t)(w1 & 0xFFFFu);
    out->cy     = (int16_t)(uint16_t)((w1 >> 16) & 0xFFFFu);
    out->conf   = bits_to_float(w2);
    out->cam_id = 0;   /* not transported */
}

void pico_link_fifo_pop(detection_t *out)
{
    uint32_t w0 = multicore_fifo_pop_blocking();
    uint32_t w1 = multicore_fifo_pop_blocking();
    uint32_t w2 = multicore_fifo_pop_blocking();
    fifo_unpack(w0, w1, w2, out);
}

bool pico_link_fifo_try_pop(detection_t *out)
{
    if (!multicore_fifo_rvalid()) return false;

    /* First word is here; remaining two from the same record are
     * already on their way (core1 pushes the trio back-to-back), so
     * we can safely use blocking pops for w1 and w2. */
    uint32_t w0 = multicore_fifo_pop_blocking();
    uint32_t w1 = multicore_fifo_pop_blocking();
    uint32_t w2 = multicore_fifo_pop_blocking();
    fifo_unpack(w0, w1, w2, out);
    return true;
}
