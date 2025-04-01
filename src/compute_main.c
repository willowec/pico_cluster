#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <i2c_fifo.h>
#include <i2c_slave.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

#include "shared.h"

#define COMPUTE_SDA_PIN 4
#define COMPUTE_SCL_PIN 5

#define ADDR_CONFIG0_PIN    16
#define ADDR_CONFIG1_PIN    17

static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    switch (event) {
    case I2C_SLAVE_RECEIVE:
        break;
    case I2C_SLAVE_REQUEST:
        i2c_write_byte(i2c, 'a');
        
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

    // power-on blink
    power_on_blink();

    sleep_ms(2000);
    setup_i2c();

    while(1)
    {
        sleep_ms(500);
        pico_set_led(true);
        sleep_ms(500);
        pico_set_led(false);
    }

    return 0;
}