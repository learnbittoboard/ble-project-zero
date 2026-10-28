/*
 * Chapter 3, step 2: debounce by timestamp.
 *
 * Same shape as step 1, plus one idea: an edge that arrives within
 * DEBOUNCE_MS of the last accepted edge is a bounce, not a new press.
 * Accepted edges submit work; the handler prints press and release.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pad_btn, LOG_LEVEL_INF);

#define DEBOUNCE_MS 50

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback button_cb_data;

/* Touched by the ISR and by main-context code: volatile and minimal. */
static volatile int64_t last_edge_ms;
static volatile int last_state;

struct press_event {
	struct k_work work;
	int state;
	int64_t at_ms;
};

static void press_work_handler(struct k_work *work)
{
	struct press_event *ev = CONTAINER_OF(work, struct press_event, work);

	LOG_INF("button %s at %lld ms", ev->state ? "pressed" : "released",
		ev->at_ms);
}

static struct press_event press_ev;
static struct press_event release_ev;

static void button_isr(const struct device *dev, struct gpio_callback *cb,
		       uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	int64_t now = k_uptime_get();
	int state = gpio_pin_get_dt(&button);

	if (state < 0 || state == last_state) {
		return; /* spurious read or no change */
	}
	if (now - last_edge_ms < DEBOUNCE_MS) {
		return; /* too soon after the last edge: a bounce */
	}

	last_edge_ms = now;
	last_state = state;

	struct press_event *ev = state ? &press_ev : &release_ev;

	ev->state = state;
	ev->at_ms = now;
	k_work_submit(&ev->work);
}

int main(void)
{
	int ret;

	k_work_init(&press_ev.work, press_work_handler);
	k_work_init(&release_ev.work, press_work_handler);
	last_edge_ms = -DEBOUNCE_MS;
	last_state = 0;

	if (!gpio_is_ready_dt(&button)) {
		LOG_ERR("button device not ready");
		return 0;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("configure failed: %d", ret);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		LOG_ERR("interrupt configure failed: %d", ret);
		return 0;
	}

	gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));
	gpio_add_callback_dt(&button, &button_cb_data);

	LOG_INF("ready: press and release button 1");
	while (1) {
		k_sleep(K_FOREVER);
	}
	return 0;
}
