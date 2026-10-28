# Step 02: The button path, end to end

The whole data path on real pins: DK button 1's interrupt submits a
work item, the work handler posts the event to a message queue, and a
consumer thread takes it out and drives LED0. Press and the LED
lights; release and it goes dark.

## Build and flash

```bash
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## What success looks like

One `button down -> LED on` line per press and one `button up -> LED
off` per release, most of the time. Sometimes one press arrives as
two or three events and the LED flickers: that is switch bounce,
arriving exactly as Chapter 2's map predicts, and Chapter 3's filter
is the cure. Type `path` in the shell to compare interrupts, work
runs and queue drops.
