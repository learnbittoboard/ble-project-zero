# Step 01: Advertiser

The DK shows up in a phone scanner app as **Project Zero**, connects,
and lights LED0 while connected. No data moves yet: this step is about
being found, and about the connect/disconnect events everything later
hangs off.

## Build and flash

```bash
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## What success looks like

The log shows `advertising as "Project Zero"`. In nRF Connect for
Mobile (or any BLE scanner) the name appears in the scan list. Connect:
LED0 lights and the log says `connected`. Disconnect: it goes out.
