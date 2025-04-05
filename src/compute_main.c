#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

//#include <i2c_fifo.h>
//#include <i2c_slave.h>
#include <pico/stdlib.h>
#include <pico/i2c_slave.h>
#include <stdio.h>
#include <string.h>

#include "shared.h"

#define COMPUTE_SDA_PIN 2
#define COMPUTE_SCL_PIN 3

#define ADDR_CONFIG0_PIN    16
#define ADDR_CONFIG1_PIN    17

#define STATE_NONE    0
#define STATE_IMAGE_RD   1
#define STATE_KERNEL_RD  2



char *input_image;
int im_width;
int im_height;

char *output_image;
signed char *kernel;
char state;

static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    uint8_t val;
    static int i;
    static int state = STATE_NONE;
    static int im_width = 0;
    static int im_height = 0;

    switch (event) {
    case I2C_SLAVE_RECEIVE:
        val = i2c_read_byte_raw(i2c);
        if (state == STATE_NONE) {
            // set state based on command
            if (val == I2C_TRANS_IMG) {
                state = STATE_IMAGE_RD;
                printf("state is reading image!\n");
                im_width = 0;
                im_height = 0;
                i = 0;
            }
        }
        else if (state == STATE_IMAGE_RD) {
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
        state = STATE_NONE;
        printf("Resetting state. w: %x, h: %x\n", im_width, im_height);
        break;
    default:
        break;
    }
}

static void setup_i2c() {
    uint8_t addr;

    // set up GPIO for address selection
    gpio_init(ADDR_CONFIG0_PIN);
    gpio_pull_up(ADDR_CONFIG0_PIN);
    gpio_init(ADDR_CONFIG1_PIN);
    gpio_pull_up(ADDR_CONFIG1_PIN);

    // read GPIO for address
    addr = COMPUTE_SLAVE_BASE_ADDRESS + 2 * gpio_get(ADDR_CONFIG0_PIN) + gpio_get(ADDR_CONFIG1_PIN);
    printf("addr: %x\n", addr);

    // set up sda
    gpio_init(COMPUTE_SDA_PIN);
    gpio_set_function(COMPUTE_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(COMPUTE_SDA_PIN);

    // set up scl
    gpio_init(COMPUTE_SCL_PIN);
    gpio_set_function(COMPUTE_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(COMPUTE_SCL_PIN);

    i2c_init(i2c1, I2C_BAUDRATE);
    // configure I2C0 for slave mode
    i2c_slave_init(i2c1, addr, &i2c_slave_handler);
}



int main() {
    uint8_t buf[64];
    uint8_t response;
    int ret;
    int i;

    // Enable UART so we can print status output
    stdio_init_all();

    // set up master i2c
    i2c_init(i2c0, I2C_BAUDRATE);
    gpio_init(12);
    gpio_init(13);
    gpio_set_function(12, GPIO_FUNC_I2C);
    gpio_set_function(13, GPIO_FUNC_I2C);
    gpio_pull_up(12);
    gpio_pull_up(13);

    sleep_ms(1000);
    power_on_blink();

    // set up slave i2c
    setup_i2c();

    buf[0] = I2C_TRANS_IMG;

    // split width and height integers for sending
    im_width = 0xABCDef01;
    im_height = 0x12345678;
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

    ret = i2c_write_blocking(i2c0, COMPUTE_SLAVE_BASE_ADDRESS, buf, 9, true);
    printf("Transmitted %d bytes\n", ret);
    ret = i2c_read_blocking(i2c0, COMPUTE_SLAVE_BASE_ADDRESS, &response, 1, false);
    printf("Read %d bytes\n", ret);

    printf("Response: %d\n", response);
    

    return 0;
}