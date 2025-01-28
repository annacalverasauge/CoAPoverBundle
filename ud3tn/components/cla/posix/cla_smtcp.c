// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "cla/cla.h"
#include "cla/mtcp_proto.h"
#include "cla/posix/cla_mtcp.h"
#include "cla/posix/cla_smtcp.h"
#include "cla/posix/cla_tcp_common.h"
#include "cla/posix/cla_tcp_util.h"

#include "bundle6/parser.h"
#include "bundle7/parser.h"

#include "platform/hal_io.h"
#include "platform/hal_task.h"

#include "ud3tn/bundle_processor.h"
#include "ud3tn/cmdline.h"
#include "ud3tn/common.h"
#include "ud3tn/result.h"

#include <sys/socket.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static const char *CLA_NAME = "smtcp";

static void smtcp_link_creation_task(void *param)
{
	struct cla_tcp_single_config *const smtcp_config = param;

	LOGF_INFO(
		"smtcp: Using %s mode",
		smtcp_config->tcp_active ? "active" : "passive"
	);

	cla_tcp_single_link_creation_task(
		smtcp_config,
		sizeof(struct mtcp_link)
	);
}

static enum ud3tn_result smtcp_launch(struct cla_config *const config)
{
	return hal_task_create(
		smtcp_link_creation_task,
		config,
		true,
		NULL
	);
}

static enum ud3tn_result smtcp_terminate(struct cla_config *const config)
{
	(void)config;
	LOG_DEBUG("SMTCP: Terminated gracefully");
	return UD3TN_OK;
}

static const char *smtcp_name_get(void)
{
	return CLA_NAME;
}

const struct cla_vtable smtcp_vtable = {
	.cla_name_get = smtcp_name_get,

	.cla_launch = smtcp_launch,
	.cla_terminate = smtcp_terminate,

	.cla_mbs_get = mtcp_mbs_get,

	.cla_get_tx_queue = cla_tcp_single_get_tx_queue,
	.cla_start_scheduled_contact = cla_tcp_single_start_scheduled_contact,
	.cla_end_scheduled_contact = cla_tcp_single_end_scheduled_contact,

	.cla_begin_packet = mtcp_begin_packet,
	.cla_end_packet = mtcp_end_packet,
	.cla_send_packet_data = mtcp_send_packet_data,

	.cla_rx_task_reset_parsers = mtcp_reset_parsers,
	.cla_rx_task_forward_to_specific_parser =
		mtcp_forward_to_specific_parser,

	.cla_read = cla_tcp_read,

	.cla_disconnect_handler = cla_tcp_single_disconnect_handler,
};

static enum ud3tn_result smtcp_init(
	struct cla_tcp_single_config *config,
	const char *node, const char *service, const bool tcp_active,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	/* Initialize base_config */
	if (cla_tcp_single_config_init(config, bundle_agent_interface)
			!= UD3TN_OK)
		return UD3TN_FAIL;

	/* set base_config vtable */
	config->base.base.vtable = &smtcp_vtable;

	config->tcp_active = tcp_active;
	config->node = strdup(node);
	config->service = strdup(service);

	return UD3TN_OK;
}

struct cla_config *smtcp_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	if (option_count < 2 || option_count > 3) {
		LOG_ERROR("smtcp: Options format has to be: <IP>,<PORT>[,<TCP_ACTIVE>]");
		return NULL;
	}

	bool tcp_active = false;

	if (option_count > 2) {
		if (parse_tcp_active(options[2], &tcp_active) != UD3TN_OK) {
			LOGF_ERROR(
				"smtcp: Could not parse TCP active flag: %s",
				options[2]
			);
			return NULL;
		}
	}

	struct cla_tcp_single_config *config =
		malloc(sizeof(struct cla_tcp_single_config));

	if (!config) {
		LOG_ERROR("smtcp: Memory allocation failed!");
		return NULL;
	}

	if (smtcp_init(config, options[0], options[1], tcp_active,
		       bundle_agent_interface) != UD3TN_OK) {
		free(config);
		LOG_ERROR("smtcp: Initialization failed!");
		return NULL;
	}

	return &config->base.base;
}
