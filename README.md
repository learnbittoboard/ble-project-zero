# BLE, Project Zero: Course Repository

Companion code for **BLE, Project Zero**, a free, project-based beginner
course from Bit to Board that builds one real product end to end: a
Bluetooth Low Energy macro pad. An nRF52840 development kit starts life
as four buttons that type letters over BLE, grows a hand-wired switch
matrix with proper scanning and debounce, learns pairing and bonding at
exactly the depth a keyboard needs, and (in an optional finale) moves
off the dev kit onto a Seeed XIAO nRF52840 with a LiPo cell as a
standalone wireless pad. The course teaches only what the project
needs: no profile-design theory, no throughput tuning, no detours.
Those live in the paid BLE ladder, and this project is the on-ramp.

## Start here

**The full guided project lives at [Bit to Board](https://bittoboard.com/courses/)** : 
this repository is its companion code, public so you can inspect exactly
what you will build before deciding anything. Bit to Board is
project-based learning: you learn embedded systems by building real
projects end to end. BLE, Project Zero is the free beginner course;
the site lists it alongside the paid courses it leads into.

- **The courses:** [Bit to Board courses](https://bittoboard.com/courses/): the guided builds this repo belongs to.
- **Free, in full:** the [Guides](https://bittoboard.com/guides/): full-depth procedures in the same style as the lessons, including [Write a Zephyr device driver from scratch](https://bittoboard.com/guides/zephyr-driver-from-scratch/) and [Create Zephyr board files from scratch](https://bittoboard.com/guides/zephyr-board-files-from-scratch/).
- **New releases and posts:** the [Bit to Board newsletter](https://bittoboard.com/newsletter/): one email when there is something worth saying.

## Structure

Each chapter's exercises live in their own folder, one folder per step:

```
ch01/step-01-hello-logging        build, flash, and read the log
ch01/step-02-shell-led            your first shell commands
ch02/step-01-button-interrupt     interrupts hand off to work
ch02/step-02-debounce             a press is a pattern, not an edge
ch03/step-01-advertiser           be seen: advertising and connections
ch04/step-01-hids-buttons         the board becomes a keyboard
ch05/step-01-bonded-keyboard      pairing, bonding, bond management
ch06/step-01-matrix-scan          the hand-wired 3 by 4 matrix, scanned
ch06/step-02-matrix-keyboard      the matrix types over the bonded link
ch07/step-01-keymap-layers        keymaps, a layer, media keys, DIS
ch08/step-01-xiao-pad             finale: the XIAO pad with a battery
```

Every step is a standalone Zephyr application with its own README: what
to build, what you should see, and why. Work through them in order : 
later steps build on the concepts (and sometimes the code) of earlier
ones.

## Pinned stack

| Piece | Version |
|---|---|
| nRF Connect SDK (NCS) | v3.4.1 |
| Zephyr | 4.4.2 (as carried by NCS v3.4.1) |
| Board | `nrf52840dk/nrf52840` (optional finale ports to the Seeed XIAO nRF52840, `xiao_ble/nrf52840`) |

## Getting started

Follow Chapter 1 of the course: install the toolchain, set up the NCS
workspace, then clone this repository into the **root of the workspace**
and name the folder `repo`:

```sh
git clone https://github.com/learnbittoboard/ble-project-zero.git repo
```

Build any step from inside the workspace, for example:

```sh
cd repo/ch01/step-01-hello-logging
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## No board? Read along

Every step's full code and expected output are in this repository and in
the lessons, so the whole project can be followed for understanding
with no hardware purchase. The board is recommended: the feel of a
real pad misbehaving on your own desk is the part reading cannot give
you: but it is not a gate.
