#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"

#include "shared.h"

#define LED_DELAY_MS    500

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
    int i, j, ret;
    int im_width, im_height, k_width, k_height, im_size, split, n_procs, color, rows;

    uint64_t start_time, op_time;
    uint64_t trans_times[4];
    uint64_t conv_times[4];
    uint64_t recv_times[4];

    // Enable UART so we can print status output
    stdio_init_all();

    // init the i2c as leader
    i2c_init(i2c_default, I2C_BAUDRATE);
    gpio_set_function(12, GPIO_FUNC_I2C);
    gpio_set_function(13, GPIO_FUNC_I2C);
    gpio_pull_up(12);
    gpio_pull_up(13);

    // power-on blink
    sleep_ms(1000);
    power_on_blink();

    while (1) {
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

        im_size = im_width*im_height*COLOR_CHANNEL_COUNT;

        // now read the image itself
        char *image_data = malloc(sizeof(char) * im_size);
        for(i=0; i<im_size; i++) {
            image_data[i] = getchar();
        }

        printf("Loaded image of size %d, shape (%d %d %d)\n", im_width*im_height*3, im_width, im_height, 3);

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
        signed char *kernel_data = malloc(sizeof(char) * k_width*k_height);
        for(i=0; i<k_width*k_height; i++) {
            kernel_data[i] = (signed char)getchar();
        }

        printf("Loaded kernel of size %d, shape (%d %d %d)\n", k_width*k_height, k_width, k_height, 1);

        // finally, read the number of procs
        read_stdio(buf, 128);
        n_procs = atoi(buf);

        printf("Using n_procs=%d\n", n_procs);
        
        /* now we have an image and a kernel. */
        /* we need to split the image up into four chunks and send the chunks along */
        /* with the kernel to the compute nodes. Over i2c. */

        // collect time for non-usb transmission
        start_time = to_us_since_boot(get_absolute_time());

        // first, prep splits
        int *im_start_idxs = malloc(n_procs * sizeof(int));
        int *im_row_counts = malloc(n_procs * sizeof(int));

        split = (im_size / n_procs) - (im_size / n_procs) % (im_width * COLOR_CHANNEL_COUNT);
        for (i=0; i<n_procs; i++) {
            // chunks should overlap by kernel size
            im_start_idxs[i] = split * i - ((k_height/2) * im_width*COLOR_CHANNEL_COUNT);
            im_row_counts[i] = (split / (im_width * COLOR_CHANNEL_COUNT)) + (k_height-1);

            // clamp bounds
            if (i == 0) {
                im_start_idxs[i] = 0;
            }
            if (i == n_procs-1) {
                im_row_counts[i] += k_height-1;
            }

            printf("sending %d: start_row=%d, rows=%d, start=%d, bytes=%d\n", i, im_start_idxs[i]/(im_width*COLOR_CHANNEL_COUNT), im_row_counts[i], im_start_idxs[i], im_row_counts[i] * im_width * COLOR_CHANNEL_COUNT);
        }

        // send image dimensions
        for (i=0; i<n_procs; i++) {
            op_time = to_us_since_boot(get_absolute_time());
            i2c_send_kim_dims(SLAVE_BASE_ADDRESS+i, im_width, im_row_counts[i], k_width, k_height);
            trans_times[i] = to_us_since_boot(get_absolute_time()) - op_time;
        }

        // wait for data to be allocated
        for (i=0; i<n_procs; i++) {
            i2c_wait_kim_dims(SLAVE_BASE_ADDRESS+i);
        }
        printf("all boards allocated...\n");

        // now transmit image data
        for (i=0; i<n_procs; i++) {
            op_time = to_us_since_boot(get_absolute_time());
            i2c_send_kim_data(SLAVE_BASE_ADDRESS+i, kernel_data, k_width, k_height, image_data+im_start_idxs[i], im_width, im_row_counts[i]); 
            trans_times[i] += to_us_since_boot(get_absolute_time()) - op_time;
        }

        // and wait for the convolutions to finish :)
        for (i=0; i<n_procs; i++) {
            i2c_wait_kim_data(SLAVE_BASE_ADDRESS+i);
        }
        printf("all boards convolved!\n");

        // get back the data! Reverse order to ensure overwrites are properly applied
        char *image_out = calloc(im_size, sizeof(char));
        if (image_out == NULL) {
            printf("ERROR out of memory!\n");
            return 1;
        }

        for (i=0; i<n_procs; i++) {
            // temporarily save *all* results into original image array (and get compute time)
            op_time = to_us_since_boot(get_absolute_time());
            conv_times[i] = i2c_request_im_data(SLAVE_BASE_ADDRESS+i, image_data, im_width, im_row_counts[i]);
            recv_times[i] = to_us_since_boot(get_absolute_time()) - op_time;

            printf("received %d: start_row=%d, rows=%d, start=%d, bytes=%d. Convolve time was %lluus (%#llx)\n", i, im_start_idxs[i]/(im_width*COLOR_CHANNEL_COUNT), im_row_counts[i], im_start_idxs[i], im_row_counts[i]*im_width*COLOR_CHANNEL_COUNT, conv_times[i], conv_times[i]);

            // copy only the used parts of the result over into output array
            im_row_counts[i] -= (k_height/2);
            memcpy(image_out+im_start_idxs[i]+im_width*(k_height/2)*COLOR_CHANNEL_COUNT, image_data+im_width*(k_height/2)*COLOR_CHANNEL_COUNT, im_width*im_row_counts[i]*COLOR_CHANNEL_COUNT);
        }
        printf("all data collected.\n");

        printf("ENDDBG\n");

        // send timing info for the run
        for (i=0; i<n_procs; i++) printf("%llu\n", trans_times[i]);
        for (i=0; i<n_procs; i++) printf("%llu\n", conv_times[i]);
        for (i=0; i<n_procs; i++) printf("%llu\n", recv_times[i]);
        printf("%llu\n", to_us_since_boot(get_absolute_time()) - start_time); // send non-usb time as well

        // send result image back to client but have a path for client to request a resend
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
                pico_set_led(true); // unrecognized command, error and exit
                return 1;
            }
        }

        // cleanup
        free(image_data);
        free(kernel_data);
        free(image_out);
        free(im_start_idxs);
        free(im_row_counts);

    }
}