/*
 * Chapter 4, step 1: the board becomes visible.
 *
 * The smallest useful BLE peripheral: it advertises with a name, and
 * when a central connects, the log says so and an LED lights. Seeing
 * your own board in a phone scanner app is the hello-world of BLE.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(advertiser, LOG_LEVEL_INF);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/*
 * The advertising data: the two facts a scanner needs first.
 * Flags say "I am a BLE-only, connectable device"; the complete name
 * is the one set in prj.conf, read back here so the two never drift.
 */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("connection failed: 0x%02x", err);
		return;
	}
	LOG_INF("connected");
	gpio_pin_set_dt(&led, 1);
}

static void adv_start(void)
{
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad),
				  NULL, 0);

	if (err) {
		LOG_ERR("advertising failed: %d", err);
		return;
	}
	LOG_INF("advertising as \"%s\": find me in a scanner app",
		CONFIG_BT_DEVICE_NAME);
}

/* Advertising stops the moment a central connects, and Zephyr does
 * not restart it for you. A peripheral that cannot be found again
 * after its first disconnect is a brick with a radio, so the
 * disconnected callback re-arms advertising from a work item (the
 * callback itself runs in a context where starting advertising
 * directly is allowed but discouraged; work keeps it simple and
 * ordered after the disconnect has fully settled).
 */
static void adv_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	adv_start();
}
static K_WORK_DEFINE(adv_work, adv_work_handler);

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	LOG_INF("disconnected: 0x%02x", reason);
	gpio_pin_set_dt(&led, 0);
	k_work_submit(&adv_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

int main(void)
{
	int err;

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED device not ready");
		return 0;
	}
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed: %d", err);
		return 0;
	}
	LOG_INF("bluetooth up");

	adv_start();

	while (1) {
		k_sleep(K_FOREVER);
	}
	return 0;
}
