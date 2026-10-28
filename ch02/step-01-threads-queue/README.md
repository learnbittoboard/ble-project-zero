# Step 01: Threads and a queue

Two threads pass "key events" through a message queue: a producer
invents a press or release every 800 ms, a consumer logs each one as
it arrives. This is the pad's data path with the hardware removed, so
the only moving parts are the kernel's: threads, priorities, and the
queue between them.

## Build and flash

```bash
west build -p always -b nrf52840dk/nrf52840 -d build .
west flash
```

## What success looks like

A new `key N down` / `key N up` line every 800 ms, each reporting it
sat in the queue for 0 ms: the consumer outranks the producer, so it
runs the moment an event lands. Type `keys` in the shell to see the
produced and consumed counters agree.
