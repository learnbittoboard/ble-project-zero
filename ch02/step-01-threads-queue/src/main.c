/*
 * Chapter 2, step 1: two threads and a queue move "key events".
 *
 * A producer thread invents a key event every 800 ms and puts it in
 * a message queue. A consumer thread takes events out and logs them.
 * No pins, no radio: this is the pad's data path with the hardware
 * removed, so the moving parts are only the kernel's.
 *
 * Watch the priorities: the consumer (5) outranks the producer (7).
 * The moment an event lands in the queue, the waiting consumer is
 * the highest-priority ready thread, so it runs before the producer
 * continues. Events are handled as they arrive, not in batches.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(ess_keys, LOG_LEVEL_INF);

/* One key event: the whole vocabulary of the pad, in one struct. */
struct key_event {
	uint8_t key;	/* which key: 1..4 */
	bool pressed;	/* true = down, false = up */
	int64_t at_ms;	/* uptime when the event was created */
};

/* A mailbox for eight events, each a copy of the struct. */
K_MSGQ_DEFINE(keyq, sizeof(struct key_event), 8, 4);

static atomic_t produced_count;
static atomic_t consumed_count;

static void producer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint32_t n = 0;

	while (1) {
		struct key_event ev = {
			.key = (uint8_t)((n / 2) % 4) + 1,
			.pressed = (n % 2) == 0,
			.at_ms = k_uptime_get(),
		};

		/* The queue copies the struct: after this call the
		 * producer may reuse or forget its own copy. K_FOREVER
		 * means "if the box is full, wait until it is not"; the
		 * consumer outranks us, so in practice it never fills.
		 */
		k_msgq_put(&keyq, &ev, K_FOREVER);
		atomic_inc(&produced_count);
		n++;
		k_msleep(800);
	}
}

static void consumer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		struct key_event ev;

		/* Sleep until an event exists. A waiting thread costs
		 * no CPU: this loop runs exactly once per event.
		 */
		k_msgq_get(&keyq, &ev, K_FOREVER);
		atomic_inc(&consumed_count);
		LOG_INF("key %u %s (sat in queue %lld ms)",
			ev.key, ev.pressed ? "down" : "up",
			(long long)(k_uptime_get() - ev.at_ms));
	}
}

K_THREAD_DEFINE(producer_id, 1024, producer_thread, NULL, NULL, NULL,
		7, 0, 0);
K_THREAD_DEFINE(consumer_id, 1024, consumer_thread, NULL, NULL, NULL,
		5, 0, 0);

static int cmd_keys(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "produced: %ld, consumed: %ld, waiting in queue: %u",
		    atomic_get(&produced_count), atomic_get(&consumed_count),
		    k_msgq_num_used_get(&keyq));
	return 0;
}

SHELL_CMD_REGISTER(keys, NULL, "Key pipeline counters", cmd_keys);

int main(void)
{
	LOG_INF("ready: producer (prio 7) and consumer (prio 5) running");
	return 0;
}
