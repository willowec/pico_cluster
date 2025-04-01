#define I2C_BAUDRATE   100000
#define COMPUTE_SLAVE_BASE_ADDRESS 0x17

/* led functions */
void pico_set_led(bool led_on);
int pico_led_init();
void power_on_blink();

#define COLOR_CHANNEL_R 0
#define COLOR_CHANNEL_G 1
#define COLOR_CHANNEL_B 2
#define COLOR_CHANNEL_COUNT 3

/* get the value of a pixel's color channel at x, y in image */
char get_pixel_channel(int x, int y, int channel, char *image_data, int im_width, int im_height);

/*
 * convolves a kernel across an image with three color channels
 */
void convolve(char *im, int im_width, int im_height, char *kernel, int k_width, int k_height, int y_start, int y_end, char *out_im);
