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

int STEP = 12;
int DIR = 13;

uint AcPins[13] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
uint32_t maskADir = (1u << 0) | (1u << 3) | (1u << 4) | (1u << 7) | (1u << 8) | (1u << 11);
uint32_t maskASpeed = (1u << 1) | (1u << 2) | (1u << 5) | (1u << 6) | (1u << 9) | (1u << 10); // Actuator GPIO speed mask
bool Actdir = 0;

/* --- Pico-link (Jetson UART) configuration --------------------------- */
#define PICO_LINK_UART      uart0
#define PICO_LINK_BAUD      115200
#define PICO_LINK_TX_PIN    16     /* GP16 = UART0 TX (unused, Jetson is read-only here) */
#define PICO_LINK_RX_PIN    17     /* GP17 = UART0 RX */
#define PICO_LINK_STALE_MS  1000   /* show "~" if no parsed line in this window */

/* Latest received detection. Updated on core0 from FIFO. */
static bin_id_t last_bin  = BIN_UNKNOWN;
static int16_t  last_cx   = 0;
static int16_t  last_cy   = 0;
static float    last_conf = 0.0f;

/* Map bin enum to a short label for the screen. */
static const char *bin_short(bin_id_t b)
{
    switch (b) {
        case BIN_METAL:   return "META";
        case BIN_GLASS:   return "GLAS";
        case BIN_PLASTIC: return "PLAS";
        case BIN_PAPER:   return "PAPR";
        case BIN_OTHER:   return "OTHR";
        default:          return "----";
    }
}

/* core1 entry: own the UART, parse lines, push to inter-core FIFO.
 * Runs forever. core0 must launch this BEFORE relying on detections. */
void core1_entry(void)
{
    pico_link_init(PICO_LINK_UART, PICO_LINK_BAUD,
                   PICO_LINK_TX_PIN, PICO_LINK_RX_PIN);

    detection_t det;
    while (true) {
        if (pico_link_read_blocking(&det)) {
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

void init_hw()
{
    stdio_init_all();
    spi_init(SPI_PORT, 10000000);
    gpio_set_function(SPI_RX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TX, GPIO_FUNC_SPI);
    tft_spi_init();
}

// void core1_entry() {}

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

        /* --- Pico-link: drain whatever core1 has pushed --------------
         * Loop drains all queued detections so we don't fall behind if
         * several arrived during a slow iteration (e.g. drive_stepper).
         * Only the most-recent detection is kept for the indicator. */
        detection_t det;
        while (pico_link_fifo_try_pop(&det)) {
            last_bin  = det.bin;
            last_cx   = det.cx;
            last_cy   = det.cy;
            last_conf = det.conf;
        }

        /* --- RX status indicator -------------------------------------
            _bin  = det.bin;
            last_cx   = det.cx;
            last_cy   = det.cy;
            last_conf = det.conf;
        }

        /* ─── RX status indicator ─────────────────────────────────────
         * Shows "~" if no line has been parsed in the last
         * PICO_LINK_STALE_MS. Otherwise shows the short bin name. */

        {
            uint32_t last_ms = pico_link_last_rx_ms();
            uint32_t now_ms  = to_ms_since_boot(get_absolute_time());
            bool fresh = (last_ms != 0) && ((now_ms - last_ms) < PICO_LINK_STALE_MS);

            char rx_line[24];
            if (fresh) {
                snprintf(rx_line, sizeof(rx_line), "RX:%s %d,%d %.2f",
                         bin_short(last_bin), last_cx, last_cy, last_conf);
            } else {
                snprintf(rx_line, sizeof(rx_line), "RX:~");
            }

            /* Clear the row, then draw. Width 160 covers the full screen
             * so leftover characters from previous frames don't stick. */
            fillRect(0, 65, 160, 10, ST7735_BLACK);
            drawText(0, 65, rx_line,
                     fresh ? ST7735_GREEN : ST7735_RED,
                     ST7735_BLACK, 1);
        }

        // Swap actuator directions every 10 seconds (nonblocking)
        if (Smartdelay + 10000 < to_ms_since_boot(get_absolute_time()))
        {
            if (Actdir)
            {
                gpio_clr_mask64(maskADir);
                Actdir = 0;
            }
            else
            {
                gpio_set_mask64(maskADir);
                Actdir = 1;
            }
            Smartdelay = to_ms_since_boot(get_absolute_time());
        }
    }
}