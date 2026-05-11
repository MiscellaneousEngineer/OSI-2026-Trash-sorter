/*
 * pico_link.h - UART line reader + ACK/DONE sender for messages
 *               between the Jetson TX2 and the Pico 2 (RP2350).
 *
 * Wire protocol (one ASCII line per message, '\n' terminated, no checksum):
 *
 *     RX (Jetson -> Pico):   M,<mat>,<cx>,<cy>,<conf>,<cam_id>\n
 *
 *         <mat>    1 = plastic    4 = cardboard
 *                  2 = glass      5 = metal
 *                  3 = paper      6 = other / unknown / heartbeat sentinel
 *         <cam_id> 0 = CAM1, 1 = CAM2 (uint8)
 *                  9 = sentinel: this line is a heartbeat (link-alive probe)
 *
 *     TX (Pico -> Jetson):   A,<crc>\n   (acknowledge: started sorting)
 *                            D,<crc>\n   (done: sort complete)
 *
 *         <crc>    two lowercase hex digits, XOR of all bytes in the
 *                  ACK'd RX line (excluding the trailing '\n').
 *
 * Designed to run alone on core1. Typical use:
 *
 *     // core1 entry
 *     pico_link_init(uart0, 115200, 16, 17);   // tx=GP16, rx=GP17
 *     detection_t det;
 *     while (true) {
 *         if (pico_link_read_blocking(&det)) {
 *             if (det.cam_id == HEARTBEAT_CAM_ID) continue;
 *             uint8_t crc = pico_link_last_line_crc();
 *             pico_link_send_ack(crc);
 *             save_for_done_later(crc);
 *             pico_link_fifo_push(&det);
 *         }
 *     }
 *
 *     // user's sorting script (core0), once finished:
 *     //     pico_link_send_done(saved_crc);
 */

#ifndef PICO_LINK_H
#define PICO_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hardware/uart.h"

/* Material bin classes. Integer values match the wire numbers exactly,
 * so a wire-num -> enum conversion is a single cast after a range check.
 * BIN_UNKNOWN is reserved for parse failures only; never on the wire. */
typedef enum {
    BIN_UNKNOWN   = 0,
    BIN_PLASTIC   = 1,
    BIN_GLASS     = 2,
    BIN_PAPER     = 3,
    BIN_CARDBOARD = 4,
    BIN_METAL     = 5,
    BIN_OTHER     = 6,   /* also the heartbeat material sentinel */
} bin_id_t;

/* True for sortable bins (1..5). 0 (UNKNOWN) and 6 (OTHER) are filler. */
#define BIN_IS_SORTABLE(b) ((b) >= BIN_PLASTIC && (b) <= BIN_METAL)

/* Sentinel cam_id used by the Jetson's link-keepalive heartbeat. */
#define HEARTBEAT_CAM_ID 9

/* Record types. Currently only 'M' is defined. */
typedef enum {
    MSG_NONE      = 0,
    MSG_DETECTION,   /* 'M' */
} msg_type_t;

/* A fully-parsed detection record. */
typedef struct {
    msg_type_t type;
    bin_id_t   bin;
    int16_t    cx;
    int16_t    cy;
    float      conf;
    uint8_t    cam_id;   /* 0 = CAM1, 1 = CAM2, 9 = heartbeat sentinel */
} detection_t;


/* ----- Init / RX -------------------------------------------------------- */

/* Configure the UART peripheral and pin muxing. Sets 8N1, hardware FIFO
 * enabled, no flow control. Call once before any read. */
void pico_link_init(uart_inst_t *uart, uint baud, uint tx_pin, uint rx_pin);

/* Block until one '\n'-terminated line arrives, then parse it.
 * Returns true on a fully-valid record (heartbeats included). On true,
 * the raw bytes of the line are stashed for pico_link_last_line_crc().
 * Returns false on malformed input; *out is left untouched.
 * Empty lines, lone '\r', and CRLF endings are tolerated. */
bool pico_link_read_blocking(detection_t *out);

/* Boot-ms timestamp of the last successful parse, or 0 if nothing has
 * ever come in. Used by the RX-stale indicator on the screen. */
uint32_t pico_link_last_rx_ms(void);


/* ----- Inter-core FIFO transport (3 words per record) ------------------- */

/* Pack a detection_t into the hardware FIFO. Blocks if the FIFO is full
 * (8 entries / 32 bits each = 2.6 records). Use from core1 only. */
void pico_link_fifo_push(const detection_t *det);

/* Non-blocking pop. Returns true and fills *out if a record was waiting,
 * false if the FIFO was empty. Use from core0 only. */
bool pico_link_fifo_try_pop(detection_t *out);


/* ----- ACK / DONE (Pico -> Jetson) -------------------------------------- */

/* XOR-checksum of all bytes in the buffer (newline excluded if present). */
uint8_t pico_link_compute_crc(const char *buf, size_t len);

/* CRC of the most recently successfully-parsed RX line. Reading this
 * after pico_link_read_blocking() returned true gives the value to
 * pass to send_ack/send_done. Returns 0 if no line has parsed yet. */
uint8_t pico_link_last_line_crc(void);

/* Send "A,XX\n" (ACK - sorting cycle starting). Two-hex-digit CRC. */
void pico_link_send_ack(uint8_t crc);

/* Send "D,XX\n" (DONE - sorting cycle complete). User calls this from
 * their sorting script when the cycle finishes. */
void pico_link_send_done(uint8_t crc);

#endif /* PICO_LINK_H */
