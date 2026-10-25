/*
 * Chapter 6, step 2: the matrix becomes the keyboard.
 *
 * Chapter 5's bonded keyboard, with its four buttons replaced by the
 * scanned 3 by 4 switch matrix from step 1. The scan thread keeps the
 * debounced pressed state of all twelve keys; whenever that state
 * changes, a fresh HID report is built from the keys currently down
 * and sent. Six keys at once is the HID boot report's limit: a
 * seventh simultaneous key turns the report into the standard
 * rollover error (all slots 0x01), which the lesson unpacks.
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
#include <string.h>

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

/* Send one raw key report (modifier byte, reserved byte, six key
 * slots). Callers build the slots; this only ships them.
 */
static void send_report(const uint8_t report[INPUT_REP_KEYS_LEN])
{
	if (!current_conn || !notify_on) {
		return;
	}
	int err = bt_hids_inp_rep_send(&hids_obj, current_conn, 0,
				       (uint8_t *)report, INPUT_REP_KEYS_LEN,
				       NULL);
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
 * itself (Chapter 3's lesson, applied): re-arm it after every
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
/* Matrix                                                              */

/* The grid, described once in the devicetree overlay (boards/):
 * 3 row outputs, 4 column inputs, on the Arduino header pins.
 */
#define MATRIX_NODE DT_NODELABEL(matrix)
#define ROWS 3
#define COLS 4
#define NKEYS (ROWS * COLS)

static const struct gpio_dt_spec rows[ROWS] = {
	GPIO_DT_SPEC_GET_BY_IDX(MATRIX_NODE, row_gpios, 0),
	GPIO_DT_SPEC_GET_BY_IDX(MATRIX_NODE, row_gpios, 1),
	GPIO_DT_SPEC_GET_BY_IDX(MATRIX_NODE, row_gpios, 2),
};
static const struct gpio_dt_spec cols[COLS] = {
	GPIO_DT_SPEC_GET_BY_IDX(MATRIX_NODE, col_gpios, 0),
	GPIO_DT_SPEC_GET_BY_IDX(MATRIX_NODE, col_gpios, 1),
	GPIO_DT_SPEC_GET_BY_IDX(MATRIX_NODE, col_gpios, 2),
	GPIO_DT_SPEC_GET_BY_IDX(MATRIX_NODE, col_gpios, 3),
};

/* What each position sends, row major. Letters a to l in this step;
 * Chapter 7 turns this table into a real, editable keymap.
 */
static const uint8_t keymap[NKEYS] = {
	0x04, 0x05, 0x06, 0x07, /* a b c d */
	0x08, 0x09, 0x0A, 0x0B, /* e f g h */
	0x0C, 0x0D, 0x0E, 0x0F, /* i j k l */
};

static bool key_state[NKEYS];
static bool key_cand[NKEYS];
static uint8_t key_stable[NKEYS];

/* Build the report from the keys currently down and send it if it
 * differs from the last one sent. More than six keys down is the
 * rollover error state the HID spec defines: every slot 0x01.
 */
static void report_update(void)
{
	static uint8_t last[INPUT_REP_KEYS_LEN];
	uint8_t report[INPUT_REP_KEYS_LEN] = { 0 };
	int slot = 2;

	for (int i = 0; i < NKEYS; i++) {
		if (!key_state[i]) {
			continue;
		}
		if (slot < INPUT_REP_KEYS_LEN) {
			report[slot++] = keymap[i];
		} else {
			memset(&report[2], 0x01, 6);
			break;
		}
	}
	if (memcmp(report, last, sizeof(report)) != 0) {
		memcpy(last, report, sizeof(report));
		send_report(report);
	}
}

static void scan_once(void)
{
	for (int r = 0; r < ROWS; r++) {
		gpio_pin_set_dt(&rows[r], 1);
		k_busy_wait(50);
		for (int c = 0; c < COLS; c++) {
			int i = r * COLS + c;
			bool down = gpio_pin_get_dt(&cols[c]) == 1;

			if (down == key_cand[i]) {
				if (key_stable[i] < 2) {
					key_stable[i]++;
				}
			} else {
				key_cand[i] = down;
				key_stable[i] = 0;
			}
			if (key_stable[i] == 2 && key_state[i] != key_cand[i]) {
				key_state[i] = key_cand[i];
				LOG_INF("key %d %s", i,
					key_state[i] ? "down" : "up");
				report_update();
			}
		}
		gpio_pin_set_dt(&rows[r], 0);
	}
}

static void scan_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);
	while (1) {
		scan_once();
		k_msleep(5);
	}
}
K_THREAD_DEFINE(scan_tid, 1024, scan_thread, NULL, NULL, NULL, 7, 0, 0);

static int matrix_init(void)
{
	for (int r = 0; r < ROWS; r++) {
		if (!gpio_is_ready_dt(&rows[r])) {
			return -ENODEV;
		}
		int ret = gpio_pin_configure_dt(&rows[r], GPIO_OUTPUT_INACTIVE);

		if (ret) {
			return ret;
		}
	}
	for (int c = 0; c < COLS; c++) {
		if (!gpio_is_ready_dt(&cols[c])) {
			return -ENODEV;
		}
		int ret = gpio_pin_configure_dt(&cols[c],
						GPIO_INPUT | GPIO_PULL_DOWN);
		if (ret) {
			return ret;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------ */

int main(void)
{
	int err;

	err = matrix_init();
	if (err) {
		LOG_ERR("matrix init failed: %d", err);
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
