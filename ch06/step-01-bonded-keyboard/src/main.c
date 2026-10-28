/*
 * Chapter 6, step 1: the keyboard grows up: pairing and bonding.
 *
 * Same four buttons, same letters as Chapter 5. What changes is the
 * trust model. The HID attributes now require an encrypted link, so a
 * host must pair before it can read a single keystroke, and the bond
 * is stored in flash (settings) so the pairing survives resets and
 * power loss. Two shell commands manage the stored bonds:
 *
 *   pad list      show every bonded host
 *   pad unpair    forget every bond (the recovery hatch)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <bluetooth/services/hids.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pad, LOG_LEVEL_INF);

/* The report map is the contract. This one describes the classic
 * boot keyboard layout: one modifier byte, one reserved byte, then
 * six key slots, plus the one byte of keyboard LEDs that flow back.
 */
#define INPUT_REP_KEYS_ID   1
#define INPUT_REP_KEYS_LEN  8
#define OUTPUT_REP_KEYS_ID  1
#define OUTPUT_REP_KEYS_LEN 1

BT_HIDS_DEF(hids_obj, OUTPUT_REP_KEYS_LEN, INPUT_REP_KEYS_LEN);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL)),
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, 0xC1, 0x03), /* 961: keyboard */
};

static struct bt_conn *current_conn;
static volatile bool notify_on; /* host has subscribed to key reports */

/* The HIDS library calls this when the host turns key report
 * notifications on or off. That subscription is the exact moment the
 * host is ready to receive keystrokes, and the moment it stops being
 * ready, so this flag is what send_key() trusts.
 */
static void hids_notify_handler(enum bt_hids_notify_evt evt)
{
	notify_on = (evt == BT_HIDS_CCCD_EVT_NOTIFY_ENABLED);
	LOG_INF("key report notifications %s",
		notify_on ? "on" : "off");
}

/* ------------------------------------------------------------------ */
/* HID                                                                 */

static void hids_pm_evt_handler(enum bt_hids_pm_evt evt,
				struct bt_conn *conn)
{
	ARG_UNUSED(conn);
	if (evt == BT_HIDS_PM_EVT_BOOT_MODE_ENTERED) {
		LOG_INF("protocol: boot mode");
	} else if (evt == BT_HIDS_PM_EVT_REPORT_MODE_ENTERED) {
		LOG_INF("protocol: report mode");
	}
}

static void hid_init(void)
{
	int err;
	struct bt_hids_init_param param = { 0 };
	struct bt_hids_inp_rep *inp;
	struct bt_hids_outp_feat_rep *outp;

	static const uint8_t report_map[] = {
		0x05, 0x01, /* Usage Page (Generic Desktop) */
		0x09, 0x06, /* Usage (Keyboard) */
		0xA1, 0x01, /* Collection (Application) */
		0x85, INPUT_REP_KEYS_ID, /* Report ID (1) */

		/* Modifier keys: 8 one bit flags (left ctrl .. right gui) */
		0x05, 0x07, /* Usage Page (Key Codes) */
		0x19, 0xE0, /* Usage Minimum (224) */
		0x29, 0xE7, /* Usage Maximum (231) */
		0x15, 0x00, /* Logical Minimum (0) */
		0x25, 0x01, /* Logical Maximum (1) */
		0x75, 0x01, /* Report Size (1) */
		0x95, 0x08, /* Report Count (8) */
		0x81, 0x02, /* Input (Data, Variable, Absolute) */

		/* Reserved byte */
		0x95, 0x01, /* Report Count (1) */
		0x75, 0x08, /* Report Size (8) */
		0x81, 0x01, /* Input (Constant) */

		/* The six key slots */
		0x95, 0x06, /* Report Count (6) */
		0x75, 0x08, /* Report Size (8) */
		0x15, 0x00, /* Logical Minimum (0) */
		0x25, 0x65, /* Logical Maximum (101) */
		0x05, 0x07, /* Usage Page (Key Codes) */
		0x19, 0x00, /* Usage Minimum (0) */
		0x29, 0x65, /* Usage Maximum (101) */
		0x81, 0x00, /* Input (Data, Array) */

		/* Keyboard LEDs coming back (num lock, caps lock, ...) */
		0x95, 0x05, /* Report Count (5) */
		0x75, 0x01, /* Report Size (1) */
		0x05, 0x08, /* Usage Page (LEDs) */
		0x19, 0x01, /* Usage Minimum (1) */
		0x29, 0x05, /* Usage Maximum (5) */
		0x91, 0x02, /* Output (Data, Variable, Absolute) */
		0x95, 0x01, /* Report Count (1) */
		0x75, 0x03, /* Report Size (3) */
		0x91, 0x01, /* Output (Constant) padding */

		0xC0 /* End Collection */
	};

	param.rep_map.data = report_map;
	param.rep_map.size = sizeof(report_map);
	param.info.bcd_hid = 0x0101;
	param.info.b_country_code = 0x00;
	param.info.flags = BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE;

	inp = &param.inp_rep_group_init.reports[0];
	inp->size = INPUT_REP_KEYS_LEN;
	inp->id = INPUT_REP_KEYS_ID;
	inp->handler = hids_notify_handler;
	param.inp_rep_group_init.cnt++;

	outp = &param.outp_rep_group_init.reports[0];
	outp->size = OUTPUT_REP_KEYS_LEN;
	outp->id = OUTPUT_REP_KEYS_ID;
	outp->handler = NULL;
	param.outp_rep_group_init.cnt++;

	param.is_kb = true;
	param.pm_evt_handler = hids_pm_evt_handler;

	err = bt_hids_init(&hids_obj, &param);
	__ASSERT(err == 0, "bt_hids_init failed: %d", err);
}

/* Send one key report: the modifier byte, then up to six keycodes.
 * key = 0 sends the all released report.
 */
static void send_key(uint8_t key)
{
	uint8_t report[INPUT_REP_KEYS_LEN] = { 0 };

	if (!current_conn || !notify_on) {
		return;
	}
	report[2] = key;
	int err = bt_hids_inp_rep_send(&hids_obj, current_conn, 0,
				       report, sizeof(report), NULL);
	if (err) {
		LOG_ERR("report send failed: %d", err);
	}
}

/* ------------------------------------------------------------------ */
/* Connection                                                          */

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("connection failed: 0x%02x", err);
		return;
	}
	current_conn = bt_conn_ref(conn);
	int rc = bt_hids_connected(&hids_obj, conn);

	LOG_INF("connected (hids: %d)", rc);
}

static void adv_start(void)
{
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad),
				  NULL, 0);

	if (err) {
		LOG_ERR("advertising failed: %d", err);
		return;
	}
	LOG_INF("advertising: pair \"%s\" as a keyboard from your host",
		CONFIG_BT_DEVICE_NAME);
}

/* Advertising stops when a central connects and never restarts by
 * itself (Chapter 4's lesson, applied): re-arm it after every
 * disconnect, or the pad is findable exactly once per reset.
 */
static void adv_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	adv_start();
}
static K_WORK_DEFINE(adv_work, adv_work_handler);

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("disconnected: 0x%02x", reason);
	notify_on = false;
	bt_hids_disconnected(&hids_obj, conn);
	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
	k_work_submit(&adv_work);
}

static void security_changed(struct bt_conn *conn,
			     bt_security_t level, enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	if (err) {
		LOG_ERR("security failed: %s level %u err %d", addr, level, err);
	} else {
		LOG_INF("security changed: %s level %u", addr, level);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("pairing complete: %s (bonded: %s)", addr,
		bonded ? "yes" : "no");
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_ERR("pairing failed: %s err %d", addr, err);
}

static struct bt_conn_auth_info_cb auth_cb = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed,
};

/* ------------------------------------------------------------------ */
/* Bond management from the shell                                      */

static void bond_list_cb(const struct bt_bond_info *info, void *user_data)
{
	const struct shell *sh = user_data;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(&info->addr, addr, sizeof(addr));
	shell_print(sh, "bonded: %s", addr);
}

static int cmd_pad_list(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	bt_foreach_bond(BT_ID_DEFAULT, bond_list_cb, (void *)sh);
	return 0;
}

static int cmd_pad_unpair(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int err = bt_unpair(BT_ID_DEFAULT, NULL);

	if (err) {
		shell_error(sh, "unpair failed: %d", err);
		return err;
	}
	shell_print(sh, "all bonds forgotten; pair again from the host");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(pad_cmds,
	SHELL_CMD(list, NULL, "List bonded hosts", cmd_pad_list),
	SHELL_CMD(unpair, NULL, "Forget all bonds", cmd_pad_unpair),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(pad, &pad_cmds, "Macro pad bond commands", NULL);

/* ------------------------------------------------------------------ */
/* Buttons                                                             */

/* HID keycodes: a = 0x04, b = 0x05, c = 0x06, d = 0x07 */
static const uint8_t button_keys[4] = { 0x04, 0x05, 0x06, 0x07 };

static const struct gpio_dt_spec buttons[4] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios),
};
static struct gpio_callback button_cbs[4];

struct key_event {
	struct k_work work;
	int button;
	int state;
};
static struct key_event key_events[4];

static void key_work_handler(struct k_work *work)
{
	struct key_event *ev = CONTAINER_OF(work, struct key_event, work);

	if (ev->state) {
		LOG_INF("button %d down: sending '%c'", ev->button + 1,
			'a' + ev->button);
		send_key(button_keys[ev->button]);
	} else {
		send_key(0);
	}
}

static void button_isr(const struct device *dev, struct gpio_callback *cb,
		       uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(pins);
	for (int i = 0; i < 4; i++) {
		if (cb == &button_cbs[i]) {
			key_events[i].button = i;
			key_events[i].state = gpio_pin_get_dt(&buttons[i]);
			k_work_submit(&key_events[i].work);
			return;
		}
	}
}

static int buttons_init(void)
{
	for (int i = 0; i < 4; i++) {
		if (!gpio_is_ready_dt(&buttons[i])) {
			return -ENODEV;
		}
		int ret = gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);

		if (ret) {
			return ret;
		}
		ret = gpio_pin_interrupt_configure_dt(&buttons[i],
						      GPIO_INT_EDGE_BOTH);
		if (ret) {
			return ret;
		}
		k_work_init(&key_events[i].work, key_work_handler);
		gpio_init_callback(&button_cbs[i], button_isr,
				   BIT(buttons[i].pin));
		gpio_add_callback_dt(&buttons[i], &button_cbs[i]);
	}
	return 0;
}

/* ------------------------------------------------------------------ */

int main(void)
{
	int err;

	err = buttons_init();
	if (err) {
		LOG_ERR("buttons init failed: %d", err);
		return 0;
	}

	hid_init();

	err = bt_conn_auth_info_cb_register(&auth_cb);
	if (err) {
		LOG_ERR("auth cb register failed: %d", err);
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed: %d", err);
		return 0;
	}
	LOG_INF("bluetooth up");

	/* Bonds live in the settings partition. Loading them after
	 * bt_enable is what makes a pairing from yesterday still count
	 * after today's reset.
	 */
	err = settings_load();
	if (err) {
		LOG_ERR("settings_load failed: %d", err);
	}

	adv_start();

	while (1) {
		k_sleep(K_FOREVER);
	}
	return 0;
}
