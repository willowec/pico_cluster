#define I2C_BAUDRATE   100000
#define COMPUTE_SLAVE_BASE_ADDRESS 0x17

#define I2C_TRANS_KIM_DIMS  0xf0    /* begin transmitting kernel and image dimensions (4 ints )*/
#define I2C_TRANS_KIM       0xf2    /* begin transmitting kernel and image data */
#define I2C_TRANS_RQST_IM   0xf3    /* request output image from compute node */

#define COMPUTE_STATE_BAD       -1
#define COMPUTE_STATE_IDLE      0
#define COMPUTE_STATE_DIMS_RD   1
#define COMPUTE_STATE_DATA_RD   2
#define COMPUTE_STATE_TRANS_RES 4
#define COMPUTE_STATE_ALLOCATE  5
#define COMPUTE_STATE_CONVOLVE  6

/* led functions */
void pico_set_led(bool led_on);
int pico_led_init();
void power_on_blink();

/* i2c functions */
void scan_i2c_bus();


#define COLOR_CHANNEL_R 0
#define COLOR_CHANNEL_G 1
#define COLOR_CHANNEL_B 2
#define COLOR_CHANNEL_COUNT 3

/* get the value of a pixel's color channel at x, y in image */
char get_pixel_channel(int x, int y, int channel, char *image_data, int im_width, int im_height);

/*
 * convolves a kernel across an image with three color channels
 */
void convolve(char *im, int im_width, int im_height, signed char *kernel, int k_width, int k_height, int y_start, int y_end, char *out_im);
