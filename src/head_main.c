#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"

#include "shared.h"


// TODO slave on head
#include "pico/i2c_slave.h"

#define LED_DELAY_MS    500


// TODO slave on head

#define SLAVE_STATE_NONE    0
#define SLAVE_STATE_IMAGE_RD   1
#define SLAVE_STATE_KERNEL_RD  2

static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    uint8_t val;
    static int i;
    static int state = SLAVE_STATE_NONE;
    static int im_width = 0;
    static int im_height = 0;


    switch (event) {
    case I2C_SLAVE_RECEIVE:
        val = i2c_read_byte_raw(i2c);
        if (state == SLAVE_STATE_NONE) {
            // set state based on command
            if (val == I2C_TRANS_IMG) {
                state = SLAVE_STATE_IMAGE_RD;
                printf("state is reading image!\n");
                im_width = 0;
                im_height = 0;
                i = 0;
            }
        }
        else if (state == SLAVE_STATE_IMAGE_RD) {
            printf("i: %d. val: %#x. w: %#x. h: %#x\n", i, val, im_width, im_height);
            if (i < 4) {
                im_width |= (val << (i*8));
            }
            else {
                im_height |= (val << ((i-4)*8));
            }
            i ++;
        }

        
        break;
    case I2C_SLAVE_REQUEST:
        i2c_write_byte_raw(i2c, state);
        pico_set_led(false);
        
        break;
    case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
        state = SLAVE_STATE_NONE;
        printf("Resetting state. w: %d, h: %d\n", im_width, im_height);
        break;
    default:
        break;
    }
}

// TODO slave on head
static void setup_i2c() {

    // set up sda
    gpio_init(10);
    gpio_set_function(10, GPIO_FUNC_I2C);
    gpio_pull_up(10);

    // set up scl
    gpio_init(11);
    gpio_set_function(11, GPIO_FUNC_I2C);
    gpio_pull_up(11);

    i2c_init(i2c1, I2C_BAUDRATE);
    // configure I2C0 for slave mode
    i2c_slave_init(i2c1, COMPUTE_SLAVE_BASE_ADDRESS, &i2c_slave_handler);
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
#if 0
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

    
    /* now we have an image and a kernel. */
    /* we need to split the image up into four chunks and send the chunks along */
    /* with the kernel to the compute nodes. Over i2c. */

    // could transmit to each node only the data it needs to know about, and then reassemble on head
    // so each node gets a quarter of the data and the head keeps track of which node got which quarter
    // and re-assembles it afterwards


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
#endif
    // TODO I2C TESTING
    setup_i2c();

    sleep_ms(2000);
    scan_i2c_bus();

    buf[0] = I2C_TRANS_IMG;

    // split width and height integers for sending
    im_width = 0xABCD;
    im_height = 0xA;
    buf[1] = im_width & 0xff;
    buf[2] = (im_width >> 8) & 0xff;
    buf[3] = (im_width >> 16) & 0xff;
    buf[4] = (im_width >> 24) & 0xff;

    buf[5] = im_height & 0xff;
    buf[6] = (im_height >> 8) & 0xff;
    buf[7] = (im_height >> 16) & 0xff;
    buf[8] = (im_height >> 24) & 0xff;

    printf("sending ");
    for (i=0; i<9; i++) {
        printf("%#x ", buf[i]);
    }
    putchar('\n');

    if(i2c_write_blocking(i2c0, COMPUTE_SLAVE_BASE_ADDRESS, buf, 9, true) < 9) {
        printf("Failed to write to slave\n");
        pico_set_led(true);
    }
    
    uint8_t response = 0;
    while (response == 0) {
        i = i2c_read_blocking(i2c0, COMPUTE_SLAVE_BASE_ADDRESS, &response, 1, false);
        if(i  < 1) {
            printf("Failed to read from slave\n");
            pico_set_led(true);
        }
        printf("Got response %d\n", response);
        sleep_ms(1000);
    }
    return 0;
}