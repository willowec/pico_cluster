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
    int im_size;

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

    // read from usb stdio waiting for image transmission start
    while(1) {
        read_stdio(buf, 128);
        if (strcmp("TRANS_IMAGE", buf) == 0) {
            break; // we got em
        }
    }

    // now read the image size
    read_stdio(buf, 128);
    im_size = atoi(buf);

    char *image_data = malloc(sizeof(char) * im_size);
    if (image_data == NULL) {
        printf("unable to reserve space for image_data!\n");
        return 1;
    }

    // now read the image itself
    for(i=0; i<im_size; i++) {
        image_data[i] = getchar();
        printf("read byte %d\n", image_data[i]);
    }

    // now print what we got
    printf("Read an image of size %d:\n", im_size);
    for(i=0; i<im_size; i++) {
        printf(" %d", image_data[i]);
    }
    printf("\n");

    return 0;
}