# Step 01: Make it yours: keymaps, a layer, media keys

The final DK firmware. The twelve positions now send F13 to F23
from a keymap table, the bottom right position is a layer key, and
layer 1 carries the arrow keys plus consumer control media keys
(volume, mute, play/pause, track skip) on a second HID report. The
pad also gains a Device Information Service and names itself
**Project Zero Pad** by Bit to Board.

## Build and flash

```bash
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## What success looks like

In a key tester (or any app that shows F keys), each position
registers its F key. Hold the bottom right key and tap the top
left: the host volume rises. The other layer 1 keys skip tracks,
mute, and move the cursor with the arrows.

Then do the chapter's real exercise: edit the two tables in
`src/main.c` (`kb_map`, `cc_map`), rebuild, and make the pad send
something you actually want on your desk.
