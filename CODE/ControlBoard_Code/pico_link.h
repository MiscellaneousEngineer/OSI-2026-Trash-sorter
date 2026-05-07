

#ifndef PICO_LINK_H
#define PICO_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/uart.h"

/* Material bin classes. Integer values match the wire protocol numbers
 * exactly (1..6), so a wire-number -> enum conversion is a single cast
 * after a range check. BIN_UNKNOWN is reserved for parse failures only;
 * it never appears on the wire. */
typedef enum {
    BIN_UNKNOWN   = 0,
    BIN_PLASTIC   = 1,
    BIN_GLASS     = 2,
    BIN_PAPER     = 3,
    BIN_CARDBOARD = 4,
    BIN_METAL     = 5,
    BIN_OTHER     = 6,   /* also used as the heartbeat material sentinel */
} bin_id_t;

/* True for the five real sortable materials (1..5). False for
 * BIN_UNKNOWN (0) and BIN_OTHER (6, which doubles as the heartbeat
 * material sentinel). Use this in core1 / sorter logic to drop both
 * unknown materials and link-keepalive filler in one check. */
#define BIN_IS_SORTABLE(b)  ((b) >= BIN_PLASTIC && (b) <= BIN_METAL)

/* Sentinel cam_id used by the Jetson's link-keepalive heartbeat. Real
 * cameras report 0 or 1; anything with cam_id == HEARTBEAT_CAM_ID is
 * filler traffic and should be dropped from sorting decisions (but it
 * still counts as "link alive" for the on-screen indicator). */
#define HEARTBEAT_CAM_ID  9

/* Record types. Only 'M' (detection) is defined today; reserved for
 * future expansion (heartbeat 'H', config 'C', ...). */
typedef enum {
    MSG_NONE = 0,
    MSG_DETECTION,   /* 'M' */
} msg_type_t;

/* Fully-parsed detection record. */
typedef struct {
    msg_type_t type;
    bin_id_t   bin;
    int16_t    cx;       /* bbox center X, pixels */
    int16_t    cy;       /* bbox center Y, pixels */
    float      conf;     /* 0.0 .. 1.0 */
    uint8_t    cam_id;   /* 0 = CAM1, 1 = CAM2 */
} detection_t;


/* ----------------------------------------------------------------------
 * Reader API (called from core1)
 * ---------------------------------------------------------------------- */

/*
 * Configure the UART peripheral and pin muxing. Call once before any
 * read. Sets 8N1, FIFO on, no flow control.
 *
 * Default wiring for this control board:
 *     uart    = uart0
 *     baud    = 115200 (matches the Jetson sender)
 *     tx_pin  = 16   (GP16 = UART0 TX)  -- unused (Jetson only sends)
 *     rx_pin  = 17   (GP17 = UART0 RX)  -- this is where bytes arrive
 *
 * NOTE: GP16 is also marked as SPI_RX in CMakeLists.txt, but the ST7735
 * is write-only so MISO is never read. Calling pico_link_init() AFTER
 * init_hw() reassigns GP16's IOMUX to UART cleanly -- no other changes
 * needed.
 */
void pico_link_init(uart_inst_t *uart, uint baud, uint tx_pin, uint rx_pin);

/*
 * Block until one complete '\n'-terminated line arrives, then parse it.
 *
 * Returns:
 *     true   -> line parsed cleanly. *out filled.
 *     false  -> malformed line (bad field count, bad number, out of
 *               range, unknown record tag, unknown bin name).
 *               *out left UNTOUCHED. Caller should simply call again.
 *
 * Tolerates CR, empty lines, CRLF endings. Lines longer than the
 * internal 64-byte buffer are silently discarded up to the next
 * newline (overrun resync).
 *
 * NOT thread-safe. Intended for a single caller on core1.
 */
bool pico_link_read_blocking(detection_t *out);

/*
 * Returns the millisecond-since-boot timestamp of the most recently
 * parsed-OK line. Returns 0 if nothing has ever been received.
 *
 * Used by core0 to drive a stale/fresh UI indicator:
 *     uint32_t last = pico_link_last_rx_ms();
 *     bool fresh = (last != 0)
 *               && (to_ms_since_boot(get_absolute_time()) - last < 1000);
 *
 * Read is atomic (32-bit aligned), safe to call from core0 without a lock.
 */
uint32_t pico_link_last_rx_ms(void);


/* ----------------------------------------------------------------------
 * Hardware multicore FIFO transport
 * ----------------------------------------------------------------------
 *
 * 3 FIFO words per detection (the 8-entry hw FIFO holds ~2.6 records):
 *
 *     [ word 0 ]  bin            (low 8 bits, upper 24 bits = 0)
 *     [ word 1 ]  cx (lo16) | cy (hi16)
 *     [ word 2 ]  conf as raw IEEE-754 float bits
 *
 * Only coords/conf/type are transported -- cam_id and msg type are
 * dropped, then re-set to defaults on the pop side (cam_id=0,
 * type=MSG_DETECTION).
 */

/* core1: push one parsed detection. Blocks briefly if the FIFO is full. */
void pico_link_fifo_push(const detection_t *det);

/* core0: pop one detection. Blocks until a record is available. */
void pico_link_fifo_pop(detection_t *out);

/* core0: non-blocking pop. Returns false immediately if no record is
 * waiting. If true, *out is fully populated. */
bool pico_link_fifo_try_pop(detection_t *out);

#endif /* PICO_LINK_H */
