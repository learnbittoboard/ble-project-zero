/*
 * Chapter 2, step 2: the pad's data path, end to end, on real pins.
 *
 * Button interrupt -> work item -> message queue -> consumer thread
 * -> LED. Every stop on this chapter's map, in one small program:
 *
 * - the ISR does the minimum: a timestamp and a submit;
 * - the work handler (thread context, system workqueue) reads the
 *   pin and posts the event to the queue, without waiting;
 * - the consumer thread takes events out and drives the LED.
 *
 * There is deliberately no debounce here. A real switch bounces,
 * so one press may arrive as several events and the LED may
 * flicker. Chapter 3 exists because of what you see in this step.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(ess_path, LOG_LEVEL_INF);

static const struct gpio_dt_spec button =
	GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec led =
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static struct gpio_callback button_cb_data;

struct key_event {
	bool pressed;	/* logical pin state when the handler read it */
	int64_t at_ms;	/* uptime the ISR recorded for the edge */
};

K_MSGQ_DEFINE(keyq, sizeof(struct key_event), 8, 4);

static volatile int64_t edge_at_ms;
static atomic_t irq_count;
static atomic_t work_count;
static atomic_t drop_count;

static void path_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	struct key_event ev = {
		.pressed = gpio_pin_get_dt(&button) != 0,
		.at_ms = edge_at_ms,
	};

	atomic_inc(&work_count);
	/* K_NO_WAIT: a work handler must not block the workqueue for
	 * every other user of it. A full queue drops the event and
	 * counts the drop, where the `path` shell command shows it.
	 */
	if (k_msgq_put(&keyq, &ev, K_NO_WAIT) != 0) {
		atomic_inc(&drop_count);
	}
}

K_WORK_DEFINE(path_work, path_work_handler);

static void button_isr(const struct device *dev, struct gpio_callback *cb,
		       uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	edge_at_ms = k_uptime_get();
	atomic_inc(&irq_count);
	k_work_submit(&path_work);
}

static void consumer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		struct key_event ev;

		k_msgq_get(&keyq, &ev, K_FOREVER);
		gpio_pin_set_dt(&led, ev.pressed ? 1 : 0);
		LOG_INF("button %s -> LED %s (edge was %lld ms ago)",
			ev.pressed ? "down" : "up",
			ev.pressed ? "on" : "off",
			(long long)(k_uptime_get() - ev.at_ms));
	}
}

K_THREAD_DEFINE(consumer_id, 1024, consumer_thread, NULL, NULL, NULL,
		5, 0, 0);

static int cmd_path(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "interrupts: %ld, work runs: %ld, queue drops: %ld",
		    atomic_get(&irq_count), atomic_get(&work_count),
		    atomic_get(&drop_count));
	return 0;
}

SHELL_CMD_REGISTER(path, NULL, "Data path counters", cmd_path);

int main(void)
{
	int ret;

	if (!gpio_is_ready_dt(&button) || !gpio_is_ready_dt(&led)) {
		LOG_ERR("button or LED device not ready");
		return 0;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("button configure failed: %d", ret);
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		LOG_ERR("LED configure failed: %d", ret);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		LOG_ERR("interrupt configure failed: %d", ret);
		return 0;
	}

	gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));
	gpio_add_callback_dt(&button, &button_cb_data);

	LOG_INF("ready: press button 1 (SW0); LED0 follows it");
	while (1) {
		k_sleep(K_FOREVER);
	}
	return 0;
}
