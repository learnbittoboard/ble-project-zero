# Step 01: Hello, logging

The smallest Zephyr app that proves the whole loop: build, flash, and a
log line you can watch on the serial console.

## Build and flash

```bash
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## What success looks like

The serial log prints `Project Zero: hello from the nRF52840 DK`, then
`tick` every two seconds.
