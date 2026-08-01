#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define BUTTON_NODE DT_ALIAS (button1)

static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET (BUTTON_NODE, gpios);

static struct gpio_callback button_cb_data;
static volatile bool blink_enabled = true;

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	blink_enabled = !blink_enabled;
}

int main(void)
{
	int ret;
	

	if (!gpio_is_ready_dt(&led1)) {
		return 0;
	}
	if (!gpio_is_ready_dt(&led2)){
		return 0;
	}
	if (!gpio_is_ready_dt(&button1)){
		return 0;
	}

	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}
	ret = gpio_pin_configure_dt (&led2, GPIO_OUTPUT_ACTIVE);
	if (ret<0){
		return 0;
	}

	ret = gpio_pin_configure_dt(&button1, GPIO_INPUT);
	if (ret < 0 ){
		return 0;
	}
	gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_EDGE_TO_ACTIVE);

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button1.pin));
	gpio_add_callback(button1.port, &button_cb_data);

	while (1) {
		if (blink_enabled) {
			gpio_pin_toggle_dt(&led1);
			gpio_pin_toggle_dt(&led2);
		}
		k_msleep(500);
	}

	return 0;
}
