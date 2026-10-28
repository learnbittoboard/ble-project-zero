# Step 02: The matrix becomes the keyboard

The bonded keyboard from Chapter 6, with its four buttons replaced
by the scanned matrix from step 1. The twelve positions send the
letters a to l. The scan thread keeps the debounced state of the
whole grid; every change rebuilds the HID report from the keys
currently down.

## Build and flash

```bash
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## What success looks like

Pair once (the bond from Chapter 6 still counts). Open any text
field on the host and press switches: letters appear. Hold six
keys at once: all six type. Press a seventh while six are held:
the report becomes the HID rollover error (every slot 0x01) and
most hosts show nothing new at all. That is the spec's answer to
"too many keys", not a bug, and Chapter 7 explains the trade.
