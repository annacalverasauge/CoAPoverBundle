// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef AGENT_UTIL_H_
#define AGENT_UTIL_H_

#include "ud3tn/bundle.h"
#include "ud3tn/bundle_processor.h"

#include <stddef.h>
#include <stdint.h>

struct bundle *agent_create_bundle(const uint8_t bp_version,
	const char *local_eid, const char *sink_id, const char *destination,
	const uint64_t creation_timestamp_ms, const uint64_t sequence_number,
	const uint64_t lifetime_ms, void *payload, size_t payload_length,
	enum bundle_proc_flags flags);

// For calling from the BP thread (in the callback), not thread safe.
struct bp_context;
struct bundle *agent_create_forward_bundle_direct(
	const struct bp_context *bundle_processor_context,
	const char *local_eid,
	const uint8_t bp_version, const char *sink_id, const char *destination,
	const uint64_t creation_timestamp_s, const uint64_t sequence_number,
	const uint64_t lifetime, void *payload, size_t payload_length,
	enum bundle_proc_flags flags);

#endif // AGENT_UTIL_H_

