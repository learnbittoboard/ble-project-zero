/*
 * Chapter 3, step 1: a button edge becomes a log line, the safe way.
 *
 * The interrupt service routine (ISR) does the minimum: it submits a
 * work item. The slow work (printing) happens in the work handler,
 * which runs in thread context, where printing is legal.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pad_btn, LOG_LEVEL_INF);

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback button_cb_data;

static void pressed_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	LOG_INF("button pressed");
}

K_WORK_DEFINE(pressed_work, pressed_work_handler);

static void button_isr(const struct device *dev, struct gpio_callback *cb,
		       uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	k_work_submit(&pressed_work);
}

int main(void)
{
	int ret;

	if (!gpio_is_ready_dt(&button)) {
		LOG_ERR("button device not ready");
		return 0;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("configure failed: %d", ret);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("interrupt configure failed: %d", ret);
		return 0;
	}

	gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));
	gpio_add_callback_dt(&button, &button_cb_data);

	LOG_INF("ready: press button 1 (SW0)");
	while (1) {
		k_sleep(K_FOREVER);
	}
	return 0;
}
