# Step 02: Debounce by timestamp

One press, one line; one release, one line. Edges that land within
50 ms of the last accepted edge are discarded as bounce.

## Build and flash

```bash
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## What success looks like

Tap, mash, feather the button: every genuine press/release pair is
reported exactly once, with a millisecond timestamp.
