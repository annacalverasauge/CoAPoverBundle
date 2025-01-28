// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef INIT_H_INCLUDED
#define INIT_H_INCLUDED

#include "platform/hal_types.h"

#include "ud3tn/cmdline.h"
#include "ud3tn/result.h"

#include <stdint.h>

// Default length of the queue toward the BP.
#ifndef BUNDLE_QUEUE_LENGTH
#define BUNDLE_QUEUE_LENGTH 4000
#endif // BUNDLE_QUEUE_LENGTH

/**
 * @brief init
 * @param argc the argument count as provided to main(...)
 * @param argv the arguments as provided to main(...)
 */
void init(int argc, char *argv[]);
void start_tasks(const struct ud3tn_cmdline_options *opt);
enum ud3tn_result start_os(void);

#endif /* INIT_H_INCLUDED */
