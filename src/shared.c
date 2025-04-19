#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "shared.h"
#include <stdio.h>

int pico_led_init()
{
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
}

// Turn the led on or off
void pico_set_led(bool led_on) 
{
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

void power_on_blink() 
{
    int rc = pico_led_init();
    hard_assert(rc == PICO_OK);
    for (int i=0; i<5; i++) {
        pico_set_led(true);
        sleep_ms(100);
        pico_set_led(false);
        sleep_ms(100);
    }
}

// I2C reserves some addresses for special purposes. We exclude these from the scan.
// These are any addresses of the form 000 0xxx or 111 1xxx
bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

void scan_i2c_bus()
{
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata;
        if (reserved_addr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
    printf("Done.\n");
}

void i2c_send_kim_dims(uint8_t addr, int iw, int ih, int kw, int kh) {
    uint8_t buf[17];
    int ret;

    buf[0] = I2C_TRANS_KIM_DIMS;

    buf[1] = iw & 0xff;
    buf[2] = (iw >> 8) & 0xff;
    buf[3] = (iw >> 16) & 0xff;
    buf[4] = (iw >> 24) & 0xff;

    buf[5] = ih & 0xff;
    buf[6] = (ih >> 8) & 0xff;
    buf[7] = (ih >> 16) & 0xff;
    buf[8] = (ih >> 24) & 0xff;

    buf[9] = kw & 0xff;
    buf[10] = (kw >> 8) & 0xff;
    buf[11] = (kw >> 16) & 0xff;
    buf[12] = (kw >> 24) & 0xff;

    buf[13] = kh & 0xff;
    buf[14] = (kh >> 8) & 0xff;
    buf[15] = (kh >> 16) & 0xff;
    buf[16] = (kh >> 24) & 0xff;

    ret = i2c_write_blocking(i2c0, addr, buf, 17, false);
}

void i2c_wait_kim_dims(uint8_t addr) 
{
    char buf;
    int ret;

    // wait for complete
    buf = COMPUTE_STATE_BAD;
    while (buf != COMPUTE_STATE_IDLE) {
        sleep_ms(1);
        ret = i2c_read_blocking(i2c0, addr, &buf, 1, false);
    }
}

void i2c_send_kim_data(uint8_t addr, signed char *k, int kw, int kh, unsigned char *im, int iw, int ih) {
    char buf = I2C_TRANS_KIM;
    int ret;

    ret = i2c_write_burst_blocking(i2c0, addr, &buf, 1);
    ret = i2c_write_burst_blocking(i2c0, addr, k, kw*kh);
    ret = i2c_write_blocking(i2c0, addr, im, iw*ih*COLOR_CHANNEL_COUNT, false);
}

void i2c_wait_kim_data(uint8_t addr)
{
    char buf;
    int ret;

    // wait for complete
    buf = COMPUTE_STATE_BAD;
    while (buf != COMPUTE_STATE_IDLE) {
        sleep_ms(1);
        ret = i2c_read_blocking(i2c0, addr, &buf, 1, false);
    }
}

uint64_t i2c_request_im_data(uint8_t addr, unsigned char *out, int iw, int ih) {
    char buf = I2C_TRANS_RQST_IM;
    int ret;

    ret = i2c_write_blocking(i2c0, addr, &buf, 1, true);
    if (ret < 1) {
        printf("ERR failed write in i2c_request_im_data: %d\n", ret);
        return ret;
    }
    ret = i2c_read_blocking(i2c0, addr, out, iw*ih*COLOR_CHANNEL_COUNT, true); // hold on to bus so uint request goes thru
    if (ret < iw*ih*COLOR_CHANNEL_COUNT) {
        printf("ERR failed read in i2c_request_im_data: %d\n", ret);
    }

    return i2c_request_uint64(addr);
}

uint64_t i2c_request_uint64(uint8_t addr)
{
    uint64_t val = 0;
    int ret;
    char buf[4] = {0};

    ret = i2c_read_blocking(i2c0, addr, buf, 4, false);
    if (ret < 4) {
        printf("Failed to read a uint64 from compute\n");
    }

    val |= buf[0] & 0xff;
    val |= (buf[1] >> 8) & 0xff;
    val |= (buf[2] >> 16) & 0xff;
    val |= (buf[3] >> 24) & 0xff;

    return val;
}

void convolve(char *im, int im_width, int im_height, signed char *kernel, int k_width, int k_height, int y_start, int y_end, char *out_im)
{
    int c, x, y, kx, ky;
    int color, sum, idx;

    /* implementation based heavily on code by Professor Weaver. */

    /* bound the border */
    if (y_start < (k_height-1)/2) y_start = (k_height-1)/2;
    if (y_end > im_height - (k_height-1)/2) y_end = im_height - (k_height-1)/2;

    /* for each color channel, for each image x, y, for each kernel x, y... */
    for(c=0; c<COLOR_CHANNEL_COUNT; c++) {
        for (x = (k_width-1)/2; x < im_width - (k_width-1)/2; x++) {
            for (y = y_start; y < y_end; y++) {
                sum = 0;
                for (kx = -(k_width-1)/2; kx < (k_width-1)/2 + 1; kx++) {
                    for (ky = -(k_height-1)/2; ky < (k_height-1)/2 + 1; ky++) {
                        /* sum all the parts touched by the kernel */
                        idx = ((y+ky)*im_width*COLOR_CHANNEL_COUNT)+(x*COLOR_CHANNEL_COUNT+c+kx*COLOR_CHANNEL_COUNT);
                        color = im[idx];
                        sum += color * kernel[((ky+(k_height-1)/2)*k_height)+(kx+k_width/2)];
                    }
                }

                if (sum < 0) sum = 0;
                if (sum > 255) sum = 255;

                out_im[(y*im_width*COLOR_CHANNEL_COUNT)+x*COLOR_CHANNEL_COUNT+c] = sum;
            }
        }
    }
}
