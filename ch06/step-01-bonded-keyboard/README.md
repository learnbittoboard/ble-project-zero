# Step 01: The bonded keyboard

Chapter 5's keyboard with a trust model. The HID attributes now
require an encrypted link, so a host must pair before it can read a
single keystroke, and the bond is stored in flash so the pairing
survives resets. Two shell commands manage the stored bonds.

## Build and flash

```bash
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## What success looks like

Pair from your host's Bluetooth settings (the pad appears as
**Project Zero**). The log shows `security changed: ... level 2`
then `pairing complete: ... (bonded: yes)`. Buttons type a, b, c, d
as before. Reset the board: it reconnects to the same host with no
new pairing prompt, because the bond was loaded from flash.

In the shell:

- `pad list` prints every bonded host address.
- `pad unpair` forgets them all. Do it once, on purpose, and watch
  the host demand a fresh pairing: that is the recovery hatch every
  shipping keyboard needs, and Chapter 6 explains why.
