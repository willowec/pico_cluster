#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>
#include <pico/i2c_slave.h>
#include <pico/multicore.h>


#include "shared.h"

#define COMPUTE_SDA_PIN 2
#define COMPUTE_SCL_PIN 3

#define ADDR_CONFIG0_PIN    16
#define ADDR_CONFIG1_PIN    17


char *input_image = NULL;
char *output_image = NULL;
int im_width;
int im_height;

signed char *kernel = NULL;
int k_width;
int k_height;

static volatile int state = COMPUTE_STATE_IDLE;



static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    uint8_t val;
    static int i;

    switch (event) {
    case I2C_SLAVE_RECEIVE:
        val = i2c_read_byte_raw(i2c);
        switch (state) {
        case COMPUTE_STATE_IDLE:
            // set state based on command
            if (val == I2C_TRANS_KIM_DIMS) {
                // set up for reading kernel and image dimensions
                state = COMPUTE_STATE_DIMS_RD;
                im_width = 0;
                im_height = 0;
                k_width = 0;
                k_height = 0;
                i = 0;

                printf("Prepped to read dimensions\n");
            }
            else if (val == I2C_TRANS_KIM) {
                // set up for reading kernel and image data
                if (input_image != NULL)    free(input_image);
                if (kernel != NULL)         free(kernel);
                if (output_image != NULL)   free(output_image);
            }
            break;

        case COMPUTE_STATE_DIMS_RD:
            // receive dimensions of job
            if (i < 4)          im_width    |= (val << (i*8));
            else if (i < 8)     im_height   |= (val << ((i-4)*8));
            else if (i < 12)    k_width     |= (val << ((i-8)*8));
            else if (i < 16)    k_height    |= (val << ((i-12)*8));

            printf("%d %x %x %x %x\n", i, im_width, im_height, k_width, k_height);
            i++;
            break;

        default:
            break;
        }
        break;
    case I2C_SLAVE_REQUEST:
        i2c_write_byte_raw(i2c, state);
        
        break;
    case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
        switch (state) {
        case COMPUTE_STATE_DIMS_RD:
            // tell main to start allocating
            printf("Switching to state allocate\n");
            state = COMPUTE_STATE_ALLOCATE;
            break;

        case COMPUTE_STATE_DATA_RD:
            // tell main to start allocating
            printf("Switching to state convolve\n");
            state = COMPUTE_STATE_CONVOLVE;
            break;

        default:
            break;
        }

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


// TODO: core 1 serves as the fake head node
void core1_entry() {

    uint8_t buf[64];
    uint8_t response;
    int ret;
    int i;



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

    buf[0] = I2C_TRANS_KIM_DIMS;

    // split width and height integers for sending
    im_width = 128;
    im_height = 128;
    k_width = 5;
    k_height = 5;

    buf[1] = im_width & 0xff;
    buf[2] = (im_width >> 8) & 0xff;
    buf[3] = (im_width >> 16) & 0xff;
    buf[4] = (im_width >> 24) & 0xff;

    buf[5] = im_height & 0xff;
    buf[6] = (im_height >> 8) & 0xff;
    buf[7] = (im_height >> 16) & 0xff;
    buf[8] = (im_height >> 24) & 0xff;

    buf[9] = k_width & 0xff;
    buf[10] = (k_width >> 8) & 0xff;
    buf[11] = (k_width >> 16) & 0xff;
    buf[12] = (k_width >> 24) & 0xff;

    buf[13] = k_height & 0xff;
    buf[14] = (k_height >> 8) & 0xff;
    buf[15] = (k_height >> 16) & 0xff;
    buf[16] = (k_height >> 24) & 0xff;


    printf("Sending data...\n");
    ret = i2c_write_blocking(i2c0, COMPUTE_SLAVE_BASE_ADDRESS, buf, 17, false);
    printf("Transmitted %d bytes\n", ret);
    while (response != COMPUTE_STATE_IDLE) {
        ret = i2c_read_blocking(i2c0, COMPUTE_SLAVE_BASE_ADDRESS, &response, 1, false);
    }
    printf("Allocating must be complete\n");
    
}

int main() {
    // Enable UART so we can print status output
    stdio_init_all();

    multicore_launch_core1(core1_entry);

    while (1) {
        switch (state) {
        case COMPUTE_STATE_IDLE:
            sleep_ms(50);
            pico_set_led(true);
            sleep_ms(50);
            pico_set_led(false);
            break;

        case COMPUTE_STATE_ALLOCATE:
            // allocate image and kernel arrays
            if (input_image != NULL)    free(input_image);
            if (output_image != NULL)   free(output_image);
            if (kernel != NULL)         free(kernel);

            printf("Allocating kernel (%d), image (%d), and output image ('' '')\n", k_width*k_height, im_width*im_height*COLOR_CHANNEL_COUNT);

            input_image = malloc(im_width*im_height*COLOR_CHANNEL_COUNT);
            output_image = malloc(im_width*im_height*COLOR_CHANNEL_COUNT);
            kernel = malloc(k_width*k_height);

            printf("Success\n");
            state = COMPUTE_STATE_IDLE;

            break;

        case COMPUTE_STATE_CONVOLVE:
            // convolve on the data
            printf("convolving...\n");

            state = COMPUTE_STATE_IDLE;
            break;
        }


    }

    return 0;
}