# Step 02: Shell, LED

Adds the Zephyr shell over the same USB serial link, with three
commands: `ledon`, `ledoff`, and `uptime`. The shell is the debugging
tool this course keeps returning to.

## Build and flash

```bash
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## What success looks like

Typing `ledon` lights LED0; `uptime` prints a growing millisecond count.
