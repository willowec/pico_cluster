#include "pico/stdlib.h"
#include "shared.h"

int pico_led_init()
{
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
}

// Turn the led on or off
void pico_set_led(bool led_on) 
{
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

void power_on_blink() 
{
    int rc = pico_led_init();
    hard_assert(rc == PICO_OK);
    for (int i=0; i<5; i++) {
        pico_set_led(true);
        sleep_ms(100);
        pico_set_led(false);
        sleep_ms(100);
    }
}