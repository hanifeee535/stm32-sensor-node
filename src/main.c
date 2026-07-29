#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

int main(void)
{
	int ret1;
	int ret2;

	if (!gpio_is_ready_dt(&led1)) {
		return 0;
	}
	if (!gpio_is_ready_dt(&led2)){
		return 0;
	}

	ret1 = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	if (ret1 < 0) {
		return 0;
	}
	ret2 = gpio_pin_configure_dt (&led2, GPIO_OUTPUT_ACTIVE);
	if (ret2<0){
		return 0;
	}

	while (1) {
		gpio_pin_toggle_dt(&led1);
		gpio_pin_toggle_dt(&led2);
		k_msleep(500);
	}

	return 0;
}
