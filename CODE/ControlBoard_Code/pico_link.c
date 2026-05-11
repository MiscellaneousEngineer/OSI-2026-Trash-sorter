/*
 * pico_link.c - Implementation of the Jetson <-> Pico 2 UART link.
 *               Numeric material protocol, CRC-acknowledged.
 */

#include "pico_link.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* Protocol max line is ~30 chars; 64 = comfortable safety margin. */
#define LINE_BUF_SIZE 64

/* Module state. Single-instance, single-core (core1) by design. */
static uart_inst_t *s_uart = NULL;
static char         s_buf[LINE_BUF_SIZE];
static size_t       s_len = 0;

/* Last successfully-parsed line, kept for CRC computation. */
static char         s_last_line[LINE_BUF_SIZE];
static size_t       s_last_line_len = 0;

/* Boot-ms of the last successful parse. volatile + 32-bit aligned, so
 * cross-core reads are atomic without a lock. */
static volatile uint32_t s_last_rx_ms = 0;


/* ----- Init ------------------------------------------------------------- */
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

uint32_t pico_link_last_rx_ms(void)
{
    return s_last_rx_ms;
}


/* ----- Helpers ---------------------------------------------------------- */

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

/* Wire material number -> enum. Out-of-range -> BIN_UNKNOWN. */
static bin_id_t material_num_to_bin(long n)
{
    if (n >= BIN_PLASTIC && n <= BIN_OTHER) {
        return (bin_id_t)n;
    }
    return BIN_UNKNOWN;
}

/* Parse one '\0'-terminated line into *out. Returns true on a fully-
 * valid record, false on any malformation. Mutates the input buffer
 * (NUL-terminates fields in place). */
static bool parse_line(char *line, detection_t *out)
{
    if (line[0] == '\0') return false;

    char *p = line;
    char *tok;
    char *endp;

    /* Field 1: record tag */
    tok = next_field(&p);
    if (!tok || tok[0] != 'M' || tok[1] != '\0') return false;

    /* Field 2: material number (1..6) */
    tok = next_field(&p);
    if (!tok || tok[0] == '\0') return false;
    long mat = strtol(tok, &endp, 10);
    if (*endp != '\0') return false;
    bin_id_t bin = material_num_to_bin(mat);
    if (bin == BIN_UNKNOWN) return false;

    /* Field 3: cx */
    tok = next_field(&p);
    if (!tok || tok[0] == '\0') return false;
    long cx = strtol(tok, &endp, 10);
    if (*endp != '\0') return false;
    if (cx < INT16_MIN || cx > INT16_MAX) return false;

    /* Field 4: cy */
    tok = next_field(&p);
    if (!tok || tok[0] == '\0') return false;
    long cy = strtol(tok, &endp, 10);
    if (*endp != '\0') return false;
    if (cy < INT16_MIN || cy > INT16_MAX) return false;

    /* Field 5: confidence */
    tok = next_field(&p);
    if (!tok || tok[0] == '\0') return false;
    float conf = strtof(tok, &endp);
    if (*endp != '\0') return false;
    if (conf < 0.0f || conf > 1.0f) return false;

    /* Field 6: cam_id */
    tok = next_field(&p);
    if (!tok || tok[0] == '\0') return false;
    long cam_id = strtol(tok, &endp, 10);
    if (*endp != '\0') return false;
    if (cam_id < 0 || cam_id > 255) return false;

    /* No extra fields allowed */
    if (p != NULL) return false;

    out->type   = MSG_DETECTION;
    out->bin    = bin;
    out->cx     = (int16_t)cx;
    out->cy     = (int16_t)cy;
    out->conf   = conf;
    out->cam_id = (uint8_t)cam_id;
    return true;
}


/* ----- Reader ----------------------------------------------------------- */
bool pico_link_read_blocking(detection_t *out)
{
    /* Snapshot the unmodified line for CRC before parse_line mutates it. */
    char raw[LINE_BUF_SIZE];
    size_t raw_len = 0;

    s_len = 0;
    while (true) {
        char c = uart_getc(s_uart);

        /* Strip lone CR (CRLF tolerance). */
        if (c == '\r') continue;

        if (c == '\n') {
            s_buf[s_len] = '\0';
            raw_len = s_len;
            memcpy(raw, s_buf, raw_len);
            raw[raw_len] = '\0';

            if (s_len == 0) {
                /* Empty line. Reset and wait for next. */
                continue;
            }

            bool ok = parse_line(s_buf, out);
            s_len = 0;

            if (ok) {
                /* Stash raw line for CRC, update RX timestamp. */
                memcpy(s_last_line, raw, raw_len);
                s_last_line[raw_len] = '\0';
                s_last_line_len = raw_len;
                s_last_rx_ms = to_ms_since_boot(get_absolute_time());
            }
            return ok;
        }

        if (s_len < LINE_BUF_SIZE - 1) {
            s_buf[s_len++] = c;
        } else {
            /* Overrun: discard up to next newline (resync). */
            while (uart_getc(s_uart) != '\n') { }
            s_len = 0;
            return false;
        }
    }
}


/* ----- FIFO transport (3 words per record) ------------------------------ */

/* Word 0:  [31:24]=cam_id  [23:8]=cx  [7:0]=bin
 * Word 1:  [31:16]=cy(unused) [15:0]=cy
 * Word 2:  conf reinterpreted as uint32 */
void pico_link_fifo_push(const detection_t *det)
{
    uint32_t w0 = ((uint32_t)det->cam_id << 24)
                | (((uint32_t)(uint16_t)det->cx) << 8)
                | ((uint32_t)det->bin & 0xFF);
    uint32_t w1 = (uint32_t)(uint16_t)det->cy;
    uint32_t w2;
    memcpy(&w2, &det->conf, sizeof(w2));

    multicore_fifo_push_blocking(w0);
    multicore_fifo_push_blocking(w1);
    multicore_fifo_push_blocking(w2);
}

bool pico_link_fifo_try_pop(detection_t *out)
{
    /* Need 3 words atomically. If we don't see at least the first one
     * available, bail without disturbing the FIFO. */
    if (!multicore_fifo_rvalid()) return false;

    uint32_t w0 = multicore_fifo_pop_blocking();
    uint32_t w1 = multicore_fifo_pop_blocking();
    uint32_t w2 = multicore_fifo_pop_blocking();

    out->type   = MSG_DETECTION;
    out->bin    = (bin_id_t)(w0 & 0xFF);
    out->cx     = (int16_t)((w0 >> 8) & 0xFFFF);
    out->cam_id = (uint8_t)((w0 >> 24) & 0xFF);
    out->cy     = (int16_t)(w1 & 0xFFFF);
    memcpy(&out->conf, &w2, sizeof(out->conf));
    return true;
}


/* ----- ACK / DONE sender ------------------------------------------------ */

uint8_t pico_link_compute_crc(const char *buf, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '\n') break;   /* exclude trailing newline if present */
        crc ^= (uint8_t)c;
    }
    return crc;
}

uint8_t pico_link_last_line_crc(void)
{
    if (s_last_line_len == 0) return 0;
    return pico_link_compute_crc(s_last_line, s_last_line_len);
}

void pico_link_send_ack(uint8_t crc)
{
    if (s_uart == NULL) return;
    char out[8];
    int n = snprintf(out, sizeof(out), "A,%02x\n", crc);
    if (n > 0) uart_write_blocking(s_uart, (const uint8_t *)out, (size_t)n);
}

void pico_link_send_done(uint8_t crc)
{
    if (s_uart == NULL) return;
    char out[8];
    int n = snprintf(out, sizeof(out), "D,%02x\n", crc);
    if (n > 0) uart_write_blocking(s_uart, (const uint8_t *)out, (size_t)n);
}
