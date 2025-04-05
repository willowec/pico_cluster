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
        // we are sending the results!
        if (state == COMPUTE_STATE_TRANS_RES) {
            i2c_write_byte_raw(i2c, output_image[i]);
            i++;

            if (i == im_width*im_height*COLOR_CHANNEL_COUNT) {
                printf("[INT] finished responding!!\n");
                state = COMPUTE_STATE_IDLE;
            }
            break;
        }

        // normally just send the state
        i2c_write_byte_raw(i2c, state);
        
        break;
    case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
        switch (state) {
        case COMPUTE_STATE_DIMS_RD:
            // tell main to start allocating
            printf("[INT] Switching to state allocate\n");
            state = COMPUTE_STATE_ALLOCATE;
            break;

        case COMPUTE_STATE_DATA_RD:
            // tell main to start allocating
            printf("[INT] Switching to state convolve\n");
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

    uint8_t buf[128];
    uint8_t response;
    int ret;
    int i;
    int iw, ih, kw, kh;

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

    // split width and height integers for sending
    iw = 5;
    ih = 5;
    kw = 3;
    kh = 3;

    signed char *dummy_kernel = malloc(kw*kh);
    signed char *dummy_image = malloc(iw*ih*COLOR_CHANNEL_COUNT);

    for(i=0; i<kw*kh; i++) dummy_kernel[i] = 1;
    for(i=0; i<iw*ih*COLOR_CHANNEL_COUNT; i++) dummy_image[i] = 1;

    printf("Sending dimensions...\n");
    i2c_send_kim_dims(iw, ih, kw, kh);
    printf("Allocating must be complete\n");


    printf("Sending kim data...\n");
    i2c_send_kim_data(dummy_kernel, kw, kh, dummy_image, iw, ih);
    printf("Convolve must be complete\n");


    printf("Requesting results...\n");    
    i2c_request_im_data(buf, iw, ih);
    printf("Received %d results!\n", ret);
    for(i=0; i<iw*ih*COLOR_CHANNEL_COUNT; i++) {
        printf("%d: in=%#x out=%#x\n", i, dummy_image[i], buf[i]);
    }

}

int main() {
    int i;

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

            printf("[WORKER] Allocating kernel (%d), image (%d), and output image ('' '')\n", k_width*k_height, im_width*im_height*COLOR_CHANNEL_COUNT);

            input_image = malloc(im_width*im_height*COLOR_CHANNEL_COUNT * sizeof(unsigned char));
            output_image = calloc(im_width*im_height*COLOR_CHANNEL_COUNT, sizeof(unsigned char));
            kernel = malloc(k_width*k_height * sizeof(signed char));

            if (!input_image || !output_image || !kernel) {
                printf("[WORKER] KERNEL AND IMAGE ALLOCATION FAILED\n");

                state = COMPUTE_STATE_BAD;
                break;
            }

            state = COMPUTE_STATE_IDLE;
            break;

        case COMPUTE_STATE_CONVOLVE:
            // convolve on the data
            printf("[WORKER] convolving...\n");

            convolve(input_image, im_width, im_height, kernel, k_width, k_height, 0, im_height, output_image);

            state = COMPUTE_STATE_IDLE;
            break;
        }


    }

    return 0;
}