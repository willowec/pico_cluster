#define I2C_BAUDRATE   100000
#define COMPUTE_SLAVE_BASE_ADDRESS 0x17

void pico_set_led(bool led_on);
int pico_led_init();
void power_on_blink();