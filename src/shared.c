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


char get_pixel_channel(int x, int y, int channel, char *image_data, int im_width, int im_height /* technically unused but it feels wrong to omit */)
{
    return image_data[y*im_width*COLOR_CHANNEL_COUNT + x*COLOR_CHANNEL_COUNT + channel];
}


void convolve(char *im, int im_width, int im_height, int *kernel, int k_width, int k_height, int y_start, int y_end, char *out_im)
{
    int c, x, y, kx, ky;
    int color, sum, idx;

    /* implementation based heavily on code by Professor Weaver. */

    /* bound the border */
    if (y_start < (k_height-1)/2) y_start = (k_height-1)/2;
    if (y_end > im_height - (k_height-1)/2) y_end = im_height - (k_height-1)/2;

    //for(idx=0; idx<im_width*im_height*COLOR_CHANNEL_COUNT; idx++) printf("%d: %d\n", idx, im[idx]);
    //printf("STARTING IMAGE\n\n\n\n\n");

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
                        //printf("idx=%d, color=%d, k=%d\n", idx, color, kernel[((ky+(k_height-1)/2)*k_height)+(kx+k_width/2)]);
                        sum += color * kernel[((ky+(k_height-1)/2)*k_height)+(kx+k_width/2)];
                    }
                }
                //printf("sum: %d\n", sum);

                if (sum < 0) sum = 0;
                if (sum > 255) sum = 255;

                out_im[(y*im_width*COLOR_CHANNEL_COUNT)+x*COLOR_CHANNEL_COUNT+c] = sum;
            }
        }
    }
}
