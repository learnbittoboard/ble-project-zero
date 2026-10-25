#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(shell_led, LOG_LEVEL_INF);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static int cmd_led_on(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	gpio_pin_set_dt(&led, 1);
	shell_print(sh, "LED on");
	return 0;
}

static int cmd_led_off(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	gpio_pin_set_dt(&led, 0);
	shell_print(sh, "LED off");
	return 0;
}

static int cmd_uptime(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_print(sh, "uptime: %lld ms", k_uptime_get());
	return 0;
}

SHELL_CMD_REGISTER(ledon, NULL, "Turn LED0 on", cmd_led_on);
SHELL_CMD_REGISTER(ledoff, NULL, "Turn LED0 off", cmd_led_off);
SHELL_CMD_REGISTER(uptime, NULL, "Print uptime in ms", cmd_uptime);

int main(void)
{
	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED device not ready");
		return 0;
	}
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	LOG_INF("Type ledon, ledoff or uptime in the shell");
	return 0;
}
