# Step 01: The matrix, scanned by hand

Twelve switches wired as 3 rows by 4 columns with one diode per
switch, watched by 7 GPIO pins. The firmware drives one row at a
time, reads the columns, and debounces the whole grid the way
Chapter 3 debounced one button. No radio in this step: the log is
the output.

Wire the matrix to the DK's Arduino header first (the lesson has
the diagram): rows on A0, A1, A2; columns on A3, A4, A5, D2.

## Build and flash

```bash
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## What success looks like

Press a switch: the log prints `key down row R col C`. Release: it
prints `key up`. Hold three switches that form three corners of a
rectangle and press the fourth corner: nothing extra appears. That
missing ghost is the diodes working, and the lesson shows what the
log looks like without them.
