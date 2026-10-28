# Step 01: Off the dev kit: the XIAO pad (optional finale)

Chapter 8's firmware, running on a Seeed XIAO nRF52840 as a
standalone battery pad. The source does not change; two files
under `boards/` retarget it: an overlay that moves the matrix to
the XIAO pads and describes the battery divider, and a `.conf`
fragment that adds the ADC and Battery Service for this board.

## Build and flash

```bash
west build -p always -b xiao_ble/nrf52840 -d build-xiao .
```

Flash by the method the lesson walks through (the XIAO has no
onboard debugger; its UF2 bootloader accepts the `zephyr.uf2`
from this build, or use an external SWD probe).

The DK build still works and is still exactly Chapter 8:

```bash
west build -p always -b nrf52840dk/nrf52840 -d build-dk .
```

## What success looks like

Wire the same matrix to the XIAO pads (rows D0, D1, D2; columns
D3, D4, D5, D6), connect a LiPo to the battery pads, pair, and
type. On the host, the pad shows a battery level next to its
name: that number comes from the divider on the board, read by
the ADC once a minute and reported over the Battery Service.
The log prints `battery: <mV> mV (<pct> %)` once a minute.
