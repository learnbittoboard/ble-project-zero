/*
 * Chapter 7, step 1: the switch matrix, scanned by hand.
 *
 * Twelve switches wired as 3 rows by 4 columns, one diode per
 * switch. The firmware drives one row at a time and reads the four
 * columns, so 7 GPIO pins watch 12 keys. A press counts only after
 * it reads the same way twice in a row (the debounce rule carried
 * over from Chapter 3, now applied to a whole grid). This step only
 * logs the keys; step 2 wires the grid to the keyboard from
 * Chapter 6.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(matrixscan, LOG_LEVEL_INF);

#define MATRIX_NODE DT_NODELABEL(matrix)

#define ROWS 3
#define COLS 4

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

static bool state[ROWS][COLS];
static bool candidate[ROWS][COLS];
static uint8_t stable[ROWS][COLS];

static void scan_once(void)
{
	for (int r = 0; r < ROWS; r++) {
		gpio_pin_set_dt(&rows[r], 1);
		k_busy_wait(50);
		for (int c = 0; c < COLS; c++) {
			bool down = gpio_pin_get_dt(&cols[c]) == 1;

			if (down == candidate[r][c]) {
				if (stable[r][c] < 2) {
					stable[r][c]++;
				}
			} else {
				candidate[r][c] = down;
				stable[r][c] = 0;
			}
			if (stable[r][c] == 2 && state[r][c] != candidate[r][c]) {
				state[r][c] = candidate[r][c];
				LOG_INF("key %s row %d col %d",
					state[r][c] ? "down" : "up  ", r, c);
			}
		}
		gpio_pin_set_dt(&rows[r], 0);
	}
}

int main(void)
{
	for (int r = 0; r < ROWS; r++) {
		if (!gpio_is_ready_dt(&rows[r])) {
			LOG_ERR("row %d not ready", r);
			return 0;
		}
		gpio_pin_configure_dt(&rows[r], GPIO_OUTPUT_INACTIVE);
	}
	for (int c = 0; c < COLS; c++) {
		if (!gpio_is_ready_dt(&cols[c])) {
			LOG_ERR("col %d not ready", c);
			return 0;
		}
		gpio_pin_configure_dt(&cols[c], GPIO_INPUT | GPIO_PULL_DOWN);
	}

	LOG_INF("matrix scan running: %d x %d", ROWS, COLS);
	while (1) {
		scan_once();
		k_msleep(5);
	}
	return 0;
}
