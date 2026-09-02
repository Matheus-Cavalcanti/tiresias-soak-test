#include "board_version.h"

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(board_version, CONFIG_BOARD_VERSION_LOG_LEVEL);

int board_version_get(struct board_version *board_rev)
{
	if (board_rev == NULL) {
		return -EINVAL;
	}

	(void)snprintk(board_rev->name, sizeof(board_rev->name), "tiresias");
	board_rev->mask = BIT(0);
	board_rev->adc_reg_val = 0;

	return 0;
}

int board_version_valid_check(void)
{
	struct board_version board_rev;
	int ret;

	ret = board_version_get(&board_rev);
	if (ret) {
		return ret;
	}

	LOG_INF("Compatible board/HW version found: %s", board_rev.name);

	return 0;
}
