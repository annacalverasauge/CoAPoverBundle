// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/init.h"
#include "ud3tn/cmdline.h"
#include "ud3tn/result.h"

#include "platform/hal_io.h"
#include "platform/hal_platform.h"
#include "platform/hal_task.h"

#include "testud3tn_unity.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void testud3tn(void);

static int test_errors;

void test_task(void *args)
{
	static const char *argv[1] = { "testud3tn" };

	LOG_INFO("Starting testsuite...");
	hal_io_message_printf("\n");

	/* Disable the logger spamming out output */
	LOG_LEVEL = 0;
	/* Start Unity */
	test_errors = UnityMain(1, argv, testud3tn);
	/* Re-enable the logger */
	LOG_LEVEL = 3;

	hal_io_message_printf("\n");
	if (!test_errors) {
		LOG_INFO("uD3TN unittests succeeded.");
		exit(EXIT_SUCCESS);
	} else {
		LOGF_ERROR("uD3TN unittests resulted in %d error(s).", test_errors);
		exit(EXIT_FAILURE);
	}
}

int main(void)
{
	char *argv[1] = {"<undefined>"};

	init(1, argv);

	hal_task_create(test_task, NULL, true, NULL);

	return start_os() == UD3TN_OK ? 0 : 1;
}
