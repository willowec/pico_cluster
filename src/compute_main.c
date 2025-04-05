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

#define COMPUTE_SDA_PIN 4
#define COMPUTE_SCL_PIN 5

#define ADDR_CONFIG0_PIN    16
#define ADDR_CONFIG1_PIN    17

#define STATE_COMPUTING 1
#define STATE_DONE      2
#define STATE_ERROR     -1

char *input_image;
int im_width;
int im_height;

char *output_image;
signed char *kernel;
char state;

static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    uint8_t cmd;
    int i;
    char buf[128];

    switch (event) {
    case I2C_SLAVE_RECEIVE:
        cmd = i2c_read_byte_raw(i2c);
        if (cmd == I2C_TRANS_IMG) {
            // head is sending an image
            state = STATE_COMPUTING;
            pico_set_led(true);

            for(i=0; i<8; i++) {
                buf[i] = i2c_read_byte_raw(i2c);
            }

            // get im width, height
            im_width = 0;
            im_height = 0;
            im_width |= (buf[0] << 0);
            im_width |= (buf[1] << 8);
            im_width |= (buf[2] << 16);
            im_width |= (buf[3] << 24);
            im_height |= (buf[4] << 0);
            im_height |= (buf[5] << 8);
            im_height |= (buf[6] << 16);
            im_height |= (buf[7] << 24);

            printf("recv image (%d %d), (%#8x %#8x). ", im_width, im_height, im_width, im_height);
            for(i=0; i<8; i++) printf("%#2x ", buf[i]);
            putchar('\n');
        }
        else if (cmd == I2C_TRANS_KERNEL) {
            // head is sending a kernel
            state = STATE_ERROR;
        }     
        
        break;
    case I2C_SLAVE_REQUEST:
        i2c_write_byte_raw(i2c, state);
        pico_set_led(false);
        
        break;
    case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
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

    i2c_init(i2c0, I2C_BAUDRATE);
    // configure I2C0 for slave mode
    i2c_slave_init(i2c0, addr, &i2c_slave_handler);
}



int main() {
    // Enable UART so we can print status output
    stdio_init_all();

    // no power on blink to ensure ready for master comms
    setup_i2c();

    while(1)
    {
        sleep_ms(500);
        //pico_set_led(true);
        //sleep_ms(500);
        //pico_set_led(false);
    }

    return 0;
}