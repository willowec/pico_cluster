#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"

#include "shared.h"

#define LED_DELAY_MS    500


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

/* function that reads stdio into a buf, stopping before buflen */
void read_stdio(char *buf, int buflen)
{
    int i;
    char ch;
    for(i=0; i<buflen; i++) {
        ch = getchar();
        if ((ch=='\n') || (ch=='\r')) {
            buf[i]='\0';
            break;
        }
        buf[i]=ch;
    }
    buf[buflen-1] = '\0';
}


int main() {
	char buf[128];
    int i;
    int im_width, im_height, k_width, k_height;
    unsigned long long start_time;

    // Enable UART so we can print status output
    stdio_init_all();

    // power-on blink
    power_on_blink();

    // init the i2c as leader
    i2c_init(i2c_default, I2C_BAUDRATE);
    gpio_set_function(12, GPIO_FUNC_I2C);
    gpio_set_function(13, GPIO_FUNC_I2C);
    gpio_pull_up(12);
    gpio_pull_up(13);

    sleep_ms(1000);

    start_time = to_us_since_boot(get_absolute_time());

    /* get the image and kernel from the client over serial */

    // read from usb stdio waiting for image transmission start
    while(1) {
        read_stdio(buf, 128);
        if (strcmp("TRANS_IMAGE", buf) == 0) {
            break; // we got em
        }
    }

    // now read the image width and height
    read_stdio(buf, 128);
    im_width = atoi(buf);
    read_stdio(buf, 128);
    im_height = atoi(buf);

    // now read the image itself
    char *image_data = malloc(sizeof(char) * im_width*im_height*COLOR_CHANNEL_COUNT);
    for(i=0; i<im_width*im_height*COLOR_CHANNEL_COUNT; i++) {
        image_data[i] = getchar();
    }

    printf("Loaded image of size %d, shape (%d %d %d). Took %lluus\n", im_width*im_height*3, im_width, im_height, 3, to_us_since_boot(get_absolute_time()) - start_time);

    start_time = to_us_since_boot(get_absolute_time());
    // read from usb stdio waiting for kernel transmission start
    while(1) {
        read_stdio(buf, 128);
        if (strcmp("TRANS_KERNEL", buf) == 0) {
            break;
        }
    }

    // now read the kernel width and height
    read_stdio(buf, 128);
    k_width = atoi(buf);
    read_stdio(buf, 128);
    k_height = atoi(buf);

    // finally, allocate and read the kernel
    int *kernel_data = malloc(sizeof(int) * k_width*k_height);
    for(i=0; i<k_width*k_height; i++) {
        kernel_data[i] = (signed char)getchar();
    }

    printf("Loaded kernel of size %d, shape (%d %d %d). Took %lluus\n", k_width*k_height, k_width, k_height, 1, to_us_since_boot(get_absolute_time()) - start_time);

    // TODO TEMP: do a convolve on  the head
    char *image_out = calloc(im_width*im_height*COLOR_CHANNEL_COUNT, sizeof(char));
    convolve(image_data, im_width, im_height, kernel_data, k_width, k_height, 0, im_height, image_out);

    // send result image but have a path for client to request a resend
    while (1) {
        // send image
        pico_set_led(true);
        for(i=0; i<im_width*im_height*COLOR_CHANNEL_COUNT; i++) {
            putchar_raw(image_out[i]);
        }
        fflush(stdout);
        
        // check response
        pico_set_led(false);
        read_stdio(buf, 128);
        if (strcmp("REPEAT", buf) == 0) {
            continue; // try again
        }
        else if (strcmp("ACK", buf) == 0) {
            break; // we got em
        } else {
            pico_set_led(true); // unrecognized command
            return 1;
        }
    }
    
    /* now we have an image and a kernel. */
    /* we need to split the image up into four chunks and send the chunks along */
    /* with the kernel to the compute nodes. Over i2c. */

    // could transmit to each node only the data it needs to know about, and then reassemble on head
    // so each node gets a quarter of the data and the head keeps track of which node got which quarter
    // and re-assembles it afterwards

    return 0;
}