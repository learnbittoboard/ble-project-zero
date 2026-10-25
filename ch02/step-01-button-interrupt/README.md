# Step 01: Button interrupt

Press DK button 1 and the log says so. The interrupt itself does
nothing but submit work: the print happens later, in thread context.

## Build and flash

```bash
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## What success looks like

One `button pressed` line per press... mostly. Watch closely: hold the
button or tap it *just so* and you may see two lines for one press.
That is the bounce Chapter 2's next step removes, and this step leaves
it in on purpose so you can see the disease before the cure.
