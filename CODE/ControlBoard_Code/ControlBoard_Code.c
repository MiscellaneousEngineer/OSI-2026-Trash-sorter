#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"

#include "freq_counter.pio.h"
#include "ST7735_TFT.h"
#include "hw.h"
// #include "UI.h"
#include "Motorcontrol.h"
#include "pico_link.h"

#define FRQPIN 15
#define WINDOW_MS 10

uint T2 = 0;
uint T1 = 0;
uint T = 0;
uint F = 0;
uint Smartdelay;
uint Smartdelay1;

uint AcPins[11] = {0, 1, 4, 5, 6, 7, 8, 9, 10, 11};
uint32_t maskBCK = (1u << 1) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 9) | (1u << 11);
uint32_t maskFWD = (1u << 0) | (1u << 2) | (1u << 4) | (1u << 6) | (1u << 8) | (1u << 10); // Actuator GPIO speed mask
bool Actdir = 0;
const int FW = 0;
const int RV = 1;

int STEP = 12;
int DIR = 13;

static const uint8_t pin_map[] = {
    [1] = 1,
    [2] = 3,
    [3] = 4,
    [4] = 2,
    [5] = 5,
};
uint8_t logical_to_physical(uint8_t id)
{
    return pin_map[id]; // assumes id is bounds-checked upstream
}

/* --- Pico-link (Jetson UART) configuration --------------------------- */
#define PICO_LINK_UART uart0
#define PICO_LINK_BAUD 115200
#define PICO_LINK_TX_PIN 16     /* GP16 = UART0 TX (unused, Jetson is read-only here) */
#define PICO_LINK_RX_PIN 17     /* GP17 = UART0 RX */
#define PICO_LINK_STALE_MS 1000 /* show "~" if no parsed line in this window */

/* Loopback self-test: when 1, core0 transmits a synthetic detection out
 * GP16 every second. Wire GP16 to GP17 externally and the reader on
 * core1 will receive its own bytes back, parse them, and drive the
 * on-screen indicator. Set to 0 (or delete) before connecting the
 * Jetson, otherwise both senders will collide on the line. */
#define LOOPBACK_TEST 1

/* Latest received detection. Updated on core0 from FIFO. */
static bin_id_t last_bin = BIN_UNKNOWN;
static int16_t last_cx = 0;
static int16_t last_cy = 0;
static float last_conf = 0.0f;

/* Map bin enum to a short label for the screen. */
static const char *bin_short(bin_id_t b)
{
    switch (b)
    {
    case BIN_PLASTIC:
        return "PLAS";
    case BIN_GLASS:
        return "GLAS";
    case BIN_PAPER:
        return "PAPR";
    case BIN_CARDBOARD:
        return "CARD";
    case BIN_METAL:
        return "META";
    case BIN_OTHER:
        return "OTHR";
    default:
        return "----";
    }
}

/* core1 entry: own the UART, parse lines, push to inter-core FIFO.
 * Runs forever. core0 must launch this BEFORE relying on detections. */
static volatile uint8_t g_last_detection_crc = 0;

uint8_t get_last_detection_crc(void)
{
    return g_last_detection_crc;
}

/* core1 entry: own the UART, parse lines, ACK valid detections, push
 * the parsed record to the inter-core FIFO. Runs forever. */
void core1_entry(void)
{
    pico_link_init(PICO_LINK_UART, PICO_LINK_BAUD,
                   PICO_LINK_TX_PIN, PICO_LINK_RX_PIN);

    detection_t det;
    while (true)
    {
        if (pico_link_read_blocking(&det))
        {
            /* Heartbeats keep last_rx_ms fresh (link-alive indicator)
             * but are NOT acknowledged and NOT pushed to core0. */
            if (det.cam_id == HEARTBEAT_CAM_ID)
                continue;

            /* Stash CRC of the line we're about to ACK. Sorting code
             * on core0 will pass this back to pico_link_send_done(). */
            uint8_t crc = pico_link_last_line_crc();
            g_last_detection_crc = crc;

            /* ACK first -- the Jetson stops sending detections as soon
             * as it sees this. Then push the work item to core0. */
            pico_link_send_ack(crc);
            pico_link_fifo_push(&det);
        }
        /* malformed line -> parser already resynced, just loop */
    }
}

// 7.8cm from center of cam
void graphfreq(uint16_t x, uint16_t y, uint16_t color, uint32_t freq)
{
    const uint16_t DISPLAY_WIDTH = 128;
    const uint32_t MIN_FREQ = 4100000;
    const uint32_t MAX_FREQ = 4400000;

    // Map freq directly to halfT in pixels (inverse: higher freq = shorter halfT)
    // At MIN_FREQ -> halfT = HALF_T_MAX (stretched out, few cycles)
    // At MAX_FREQ -> halfT = HALF_T_MIN (compressed, more cycles)
    const uint16_t HALF_T_MAX = 32; // pixels per half-cycle at MIN_FREQ
    const uint16_t HALF_T_MIN = 4;  // pixels per half-cycle at MAX_FREQ

    uint16_t halfT = HALF_T_MAX - (uint16_t)((uint32_t)(freq - MIN_FREQ) * (HALF_T_MAX - HALF_T_MIN) / (MAX_FREQ - MIN_FREQ));
    if (halfT < HALF_T_MIN)
        halfT = HALF_T_MIN;
    if (halfT > HALF_T_MAX)
        halfT = HALF_T_MAX;

    // Build waveform sample array
    bool plot[DISPLAY_WIDTH];
    bool state = true;
    uint16_t index = 0;

    while (index < DISPLAY_WIDTH)
    {
        for (uint16_t i = 0; i < halfT && index < DISPLAY_WIDTH; i++)
            plot[index++] = state;
        state = !state;
    }

    // Draw waveform
    for (index = 0; index < DISPLAY_WIDTH; index++)
    {
        bool rising = (index > 0 && !plot[index - 1] && plot[index]);
        bool falling = (index > 0 && plot[index - 1] && !plot[index]);

        if (rising || falling)
        {
            for (uint16_t v = 0; v <= 20; v++)
                drawPixel(x + index, y - v, color);
        }

        uint16_t draw_y = plot[index] ? y : y - 20;
        drawPixel(x + index, draw_y, color);
    }
}

void driveActuator(uint id, bool DIR)
{
    uint8_t phys = logical_to_physical(id); // 1..5
    gpio_put(AcPins[(phys - 1) * 2], DIR);
    gpio_put(AcPins[(phys - 1) * 2 + 1], !DIR);
}

void init_hw()
{
    stdio_init_all();
    spi_init(SPI_PORT, 10000000);
    gpio_set_function(SPI_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TX, GPIO_FUNC_SPI);
    tft_spi_init();
}

int main()
{
    stdio_init_all();

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &freq_counter_program);
    uint sm = pio_claim_unused_sm(pio, true);
    freq_counter_program_init(pio, sm, offset, FRQPIN);

    init_hw();
    TFT_GreenTab_Initialize();
    fillScreen(ST7735_BLACK);

    for (uint32_t i = 0; i < 12; i++)
    {
        gpio_init(AcPins[i]);
        gpio_set_dir(AcPins[i], 1);
    }

    setup_stepper(STEP, DIR);

    /* Hand off the UART link to core1. Must come AFTER init_hw() so
     * GP16's IOMUX gets reassigned from SPI_RX to UART0 TX cleanly. */
    multicore_launch_core1(core1_entry);

    Smartdelay = to_ms_since_boot(get_absolute_time());
    Smartdelay1 = to_ms_since_boot(get_absolute_time());

    sleep_ms(200);
    // motor POST
    gpio_clr_mask64(maskFWD);
    gpio_set_mask64(maskBCK);
    sleep_ms(10000);
    gpio_clr_mask64(maskBCK);
    gpio_set_mask64(maskFWD);
    sleep_ms(10000);
    gpio_clr_mask64(maskFWD);
    gpio_clr_mask64(maskBCK);

    while (true)
    {

        T1 = pio_sm_get(pio, sm);
        sleep_ms(10);
        T2 = pio_sm_get(pio, sm);
        T = T1 - T2;
        F = T / 0.01;
        fillRect(0, 10, 160, 35, ST7735_BLACK);
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", F);
        drawText(20, 5, buffer, ST7735_WHITE, ST7735_BLACK, 1);
        graphfreq(0, 40, ST7735_WHITE, F);
        char str[32];

        snprintf(buffer, sizeof(buffer), "%d", Smartdelay + 10000 - to_ms_since_boot(get_absolute_time()));

        if (Actdir)
        {
            snprintf(str, sizeof(str), "%s%s", "Out ", buffer);
        }
        else
        {
            snprintf(str, sizeof(str), "%s%s", "In ", buffer);
        }

        drawText(0, 50, str, ST7735_WHITE, ST7735_BLACK, 1);

#if LOOPBACK_TEST
        /* --- Loopback test: send one synthetic detection per second --
         * Cycles through all six bin numbers + one heartbeat so you can
         * see the parser decode each. The heartbeat (cam_id=9) should
         * NOT update the on-screen detection - only the link timestamp.
         * Wire GP16 to GP17 externally for it to come back. */
        {
            static uint32_t t_test = 0;
            static uint8_t test_idx = 0;
            static const char *test_lines[7] = {
                "M,1,100,200,0.950,0\n", /* plastic   */
                "M,2,512,180,0.610,1\n", /* glass     */
                "M,3,98,401,0.730,0\n",  /* paper     */
                "M,4,250,300,0.820,1\n", /* cardboard */
                "M,5,320,240,0.870,0\n", /* metal     */
                "M,6,50,50,0.400,1\n",   /* other     */
                "M,6,800,800,0.000,9\n", /* heartbeat */
            };
            uint32_t now_ms = to_ms_since_boot(get_absolute_time());
            if (now_ms > t_test + 1000)
            {
                uart_puts(PICO_LINK_UART, test_lines[test_idx]);
                test_idx = (test_idx + 1) % 7;
                t_test = now_ms;
            }
        }
#endif

        /* --- Pico-link: drain whatever core1 has pushed --------------
         * Loop drains all queued detections so we don't fall behind if
         * several arrived during a slow iteration (e.g. drive_stepper).
         * Only the most-recent detection is kept for the indicator. */
        detection_t det;
        while (pico_link_fifo_try_pop(&det))
        {
            last_bin = det.bin;
            last_cx = det.cx;
            last_cy = det.cy;
            last_conf = det.conf;

            if (BIN_IS_SORTABLE(det.bin))
            {
                /* DEMO WORKAROUND: physical actuator #1 (plastic) is dead.
                 * Divert plastic detections -> paper bin (logical 3).
                 * Divert paper   detections -> cardboard bin (logical 4).
                 * Applied BEFORE stepper + actuator so both stay in sync.
                 * NOTE: paper and cardboard items share the cardboard bin. */
                uint8_t routed_bin = det.bin;
                if (det.bin == 1)
                    routed_bin = 3; /* plastic -> paper    */
                else if (det.bin == 3)
                    routed_bin = 4; /* paper   -> cardboard */

                if (routed_bin == 2){
                drive_stepper(((routed_bin - 1) * 1050) + 800, 0.5, STEP, DIR, 1);
                driveActuator(routed_bin, FW);
                sleep_ms(20000);
                driveActuator(routed_bin, RV);
                }else{
                drive_stepper(((routed_bin - 1) * 1050) + 800, 0.5, STEP, DIR, 1);
                driveActuator(routed_bin, FW);
                sleep_ms(10000);
                driveActuator(routed_bin, RV);
                }
                pico_link_send_done(get_last_detection_crc());
            }
        }

        /* --- RX status indicator -------------------------------------
         * Shows "~" if no line has been parsed in the last
         * PICO_LINK_STALE_MS. Otherwise shows the short bin name. */
        {
            uint32_t last_ms = pico_link_last_rx_ms();
            uint32_t now_ms = to_ms_since_boot(get_absolute_time());
            bool fresh = (last_ms != 0) && ((now_ms - last_ms) < PICO_LINK_STALE_MS);

            char rx_line[24];
            if (fresh)
            {
                snprintf(rx_line, sizeof(rx_line), "RX:%s %d,%d %.2f",
                         bin_short(last_bin), last_cx, last_cy, last_conf);
            }
            else
            {
                snprintf(rx_line, sizeof(rx_line), "RX:~");
            }

            /* Clear the row, then draw. Width 160 covers the full screen
             * so leftover characters from previous frames don't stick. */
            fillRect(0, 65, 160, 10, ST7735_BLACK);
            drawText(0, 65, rx_line,
                     fresh ? ST7735_GREEN : ST7735_RED,
                     ST7735_BLACK, 1);
        }

        /*    for (int i = 1; i <= 5; i++)
            {
                driveActuator(i, FW);
                sleep_ms(1000);
                driveActuator(i, RV);
                sleep_ms(1000);
                uint8_t phys = logical_to_physical(i);
                gpio_put(AcPins[(phys - 1) * 2], 0);
                gpio_put(AcPins[(phys - 1) * 2 + 1], 0);
            }*/

        // Swap actuator directions every 10 seconds (nonblocking)
        /*if (Smartdelay + 10000 < to_ms_since_boot(get_absolute_time()))
        {
            if (Actdir)
            {
                gpio_clr_mask64(maskBCK);
                gpio_set_mask64(maskFWD);
                Actdir = 0;
            }
            else
            {
                gpio_clr_mask64(maskFWD);
                gpio_set_mask64(maskBCK);
                Actdir = 1;
            }
            Smartdelay = to_ms_since_boot(get_absolute_time());
        }

        if (Smartdelay1 + 2000 < to_ms_since_boot(get_absolute_time()))
        {
            drive_stepper(1050, 0.5, STEP, DIR, 1);
            Smartdelay1 = to_ms_since_boot(get_absolute_time());
        }*/
    }
}