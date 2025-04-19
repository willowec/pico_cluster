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

#define LED_BLUE_PIN    0
#define LED_YELLOW_PIN  1

char *input_image = NULL;
char *output_image = NULL;
int im_width;
int im_height;

signed char *kernel = NULL;
int k_width;
int k_height;

uint64_t op_time = 0;

static volatile int state = COMPUTE_STATE_IDLE;


static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    uint8_t val;
    static int i;

    gpio_put(LED_YELLOW_PIN, 1);

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

                printf("[INT] Prepped to read dimensions\n");
            }
            else if (val == I2C_TRANS_KIM) {
                // set up for reading kernel and image data
                state = COMPUTE_STATE_DATA_RD;
                i = 0;
    
                printf("[INT] Prepped to read image data\n");
            }
            else if (val == I2C_TRANS_RQST_IM) {
                // set up for responding with the results
                state = COMPUTE_STATE_TRANS_RES;
                i = 0;

                printf("[INT] Prepped to respond with image data\n");

            }
            break;

        case COMPUTE_STATE_DIMS_RD:
            // receive dimensions of job
            if (i < 4)          im_width    |= (val << (i*8));
            else if (i < 8)     im_height   |= (val << ((i-4)*8));
            else if (i < 12)    k_width     |= (val << ((i-8)*8));
            else if (i < 16)    k_height    |= (val << ((i-12)*8));

            i++;
            break;

        case COMPUTE_STATE_DATA_RD:
            // receive image data
            if (i < k_width*k_height) {
                kernel[i] = (signed char)val;
            }
            else if (i < k_width*k_height*im_width*im_height*COLOR_CHANNEL_COUNT) {
                input_image[i - k_width*k_height] = (unsigned char)val;
            }
            else {
                printf("[INT] Sent too much data! i=%d\n", i);
            }

            i++;
            break;

        default:
            break;
        }
        break;
    case I2C_SLAVE_REQUEST:
        if (state == COMPUTE_STATE_TRANS_RES) {
            // we are sending the results!

            if (i < im_width*im_height*COLOR_CHANNEL_COUNT) {
                // transmit image contents
                i2c_write_byte_raw(i2c, output_image[i]);
                if (i == im_width*im_height*COLOR_CHANNEL_COUNT-1) {
                    gpio_put(LED_BLUE_PIN, 1);
                }
            }
            else {
                // finish it off with the 64 bit convolve runtime
                i2c_write_byte_raw(i2c, op_time >> ((i-im_width*im_height*COLOR_CHANNEL_COUNT) * 8) & 0xff);
            }

            i++;
        } 
        else {
            // normally just send the state
            i2c_write_byte_raw(i2c, state);
        }
        
        break;
    case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
        gpio_put(LED_YELLOW_PIN, 0);

        switch (state) {
        case COMPUTE_STATE_DIMS_RD:
            // tell main to start allocating
            printf("[INT] Switching to state allocate\n");
            state = COMPUTE_STATE_ALLOCATE;
            break;

        case COMPUTE_STATE_DATA_RD:
            // tell main to start convolving
            printf("[INT] Switching to state convolve\n");
            state = COMPUTE_STATE_CONVOLVE;
            break;

        // TODO: this breaks comms
        case COMPUTE_STATE_TRANS_RES:
            if (i != 0) {
                state = COMPUTE_STATE_IDLE;
            }
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
    addr = SLAVE_BASE_ADDRESS + 2 * gpio_get(ADDR_CONFIG0_PIN) + gpio_get(ADDR_CONFIG1_PIN);
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
    uint64_t start_time;

    // Enable UART so we can print status output
    stdio_init_all();

    // set up slave i2c
    setup_i2c();

    // set up LEDs
    gpio_init(LED_YELLOW_PIN);
    gpio_set_dir(LED_YELLOW_PIN, GPIO_OUT);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);

    power_on_blink();

    // main loop
    while (1) {
        switch (state) {
        case COMPUTE_STATE_IDLE:
            // blink rate needs to be really fast so as to reduce latency exiting idle
            sleep_ms(1);
            pico_set_led(true);
            sleep_ms(1);
            pico_set_led(false);
            break;

        case COMPUTE_STATE_ALLOCATE:
            gpio_put(LED_BLUE_PIN, 1);

            // allocate image and kernel arrays
            if (input_image != NULL)    free(input_image);
            if (output_image != NULL)   free(output_image);
            if (kernel != NULL)         free(kernel);

            printf("[WORKER] Allocating kernel (%d), image (%d), and output image ('' '')\n", k_width*k_height, im_width*im_height*COLOR_CHANNEL_COUNT);

            input_image = malloc(im_width*im_height*COLOR_CHANNEL_COUNT * sizeof(unsigned char));
            output_image = calloc(im_width*im_height*COLOR_CHANNEL_COUNT, sizeof(unsigned char));
            kernel = malloc(k_width*k_height * sizeof(signed char));

            if (!input_image || !output_image || !kernel) {
                printf("[WORKER] KERNEL AND IMAGE ALLOCATION FAILED\n");

                state = COMPUTE_STATE_BAD;
                break;
            }

            gpio_put(LED_BLUE_PIN, 0);
            state = COMPUTE_STATE_IDLE;
            break;

        case COMPUTE_STATE_CONVOLVE:
            gpio_put(LED_BLUE_PIN, 1);

            printf("[WORKER] convolving...\n");
            
            // convolve on the data
            start_time = to_us_since_boot(get_absolute_time());
            convolve(input_image, im_width, im_height, kernel, k_width, k_height, 0, im_height, output_image);
            op_time = to_us_since_boot(get_absolute_time()) - start_time;

            gpio_put(LED_BLUE_PIN, 0);
            state = COMPUTE_STATE_IDLE;
            break;
        }
    }

    return 0;
}