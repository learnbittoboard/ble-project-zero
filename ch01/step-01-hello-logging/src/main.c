#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hello, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("Project Zero: hello from the nRF52840 DK");
	while (1) {
		LOG_INF("tick");
		k_sleep(K_SECONDS(2));
	}
	return 0;
}
