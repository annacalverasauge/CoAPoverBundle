// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "cla/cla.h"
#include "cla/cla_contact_tx_task.h"
#include "cla/posix/cla_tcp_common.h"
#include "cla/posix/cla_tcp_util.h"

#include "platform/hal_io.h"
#include "platform/hal_semaphore.h"
#include "platform/hal_task.h"
#include "platform/hal_time.h"

#include "ud3tn/common.h"
#include "ud3tn/result.h"

#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#ifndef ENONET
#define ENONET 64
#endif // ENONET

#ifndef EHOSTDOWN
#define EHOSTDOWN 112
#endif // EHOSTDOWN

enum ud3tn_result cla_tcp_config_init(
	struct cla_tcp_config *config,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	if (cla_config_init(&config->base, bundle_agent_interface) != UD3TN_OK)
		return UD3TN_FAIL;

	config->socket = -1;

	return UD3TN_OK;
}

enum ud3tn_result cla_tcp_single_config_init(
	struct cla_tcp_single_config *config,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	if (cla_tcp_config_init(&config->base,
				bundle_agent_interface) != UD3TN_OK)
		return UD3TN_FAIL;

	config->link = NULL;
	config->num_active_contacts = 0;
	config->rl_config.last_connection_attempt_ms = 0;
	config->rl_config.last_connection_attempt_no = 1;
	config->contact_activity_sem = hal_semaphore_init_binary();
	if (!config->contact_activity_sem) {
		LOG_ERROR("TCP: Cannot allocate memory for contact activity semaphore!");
		return UD3TN_FAIL;
	}

	return UD3TN_OK;
}

enum ud3tn_result cla_tcp_link_init(
	struct cla_tcp_link *link, int connected_socket,
	struct cla_tcp_config *config,
	const char *const cla_addr,
	const bool is_tx)
{
	ASSERT(connected_socket >= 0);
	link->connection_socket = connected_socket;

	// This will fire up the RX and TX tasks
	// NOTE: A TCP link _always_ needs an RX task to detect when the
	// connection has been closed or reset.
	if (cla_link_init(&link->base, &config->base, cla_addr, true, is_tx)
			!= UD3TN_OK)
		return UD3TN_FAIL;

	// If termination of the RX task is already requested (e.g., if spawning
	// the TX task failed), allow it to exit by closing the socket.
	if (hal_semaphore_is_blocked(link->base.rx_task_notification))
		close(link->connection_socket);

	return UD3TN_OK;
}

enum ud3tn_result cla_tcp_read(struct cla_link *link,
			       uint8_t *buffer, size_t length,
			       size_t *bytes_read)
{
	struct cla_tcp_link *tcp_link = (struct cla_tcp_link *)link;
	ssize_t ret;

	do {
		ret = recv(
			tcp_link->connection_socket,
			buffer,
			length,
			0
		);
	} while (ret == -1 && errno == EINTR);

	if (ret < 0) {
		LOG_ERRNO_INFO("TCP", "recv()", errno);
		link->config->vtable->cla_disconnect_handler(link);
		return UD3TN_FAIL;
	} else if (ret == 0) {
		LOGF_INFO("TCP: A peer (via CLA %s) has disconnected gracefully!",
		     link->config->vtable->cla_name_get());
		link->config->vtable->cla_disconnect_handler(link);
		return UD3TN_FAIL;
	}
	if (bytes_read)
		*bytes_read = ret;
	return UD3TN_OK;
}

enum ud3tn_result cla_tcp_connect(struct cla_tcp_config *const config,
				  const char *node, const char *service)
{
	if (node == NULL || service == NULL)
		return UD3TN_FAIL;

	config->socket = create_tcp_socket(
		node,
		service,
		true,
		NULL
	);

	if (config->socket < 0)
		return UD3TN_FAIL;

	LOGF_INFO(
		"TCP: CLA %s is now connected to [%s]:%s",
		config->base.vtable->cla_name_get(),
		node,
		service
	);

	return UD3TN_OK;
}

enum ud3tn_result cla_tcp_listen(struct cla_tcp_config *config,
				 const char *node, const char *service,
				 int backlog)
{
	if (node == NULL || service == NULL)
		return UD3TN_FAIL;

	config->socket = create_tcp_socket(
		node,
		service,
		false,
		NULL
	);

	if (config->socket < 0)
		return UD3TN_FAIL;

	// Listen for incoming connections.
	if (listen(config->socket, backlog) < 0) {
		LOG_ERRNO("TCP", "listen()", errno);
		close(config->socket);
		config->socket = -1;
		return UD3TN_FAIL;
	}

	LOGF_INFO(
		"TCP: CLA %s is now listening on [%s]:%s",
		config->base.vtable->cla_name_get(),
		node,
		service
	);

	return UD3TN_OK;
}

int cla_tcp_accept_from_socket(struct cla_tcp_config *config,
			       const int listener_socket,
			       char **const addr)
{
	const int enable = 1;
	struct sockaddr_storage sockaddr_tmp;
	socklen_t sockaddr_tmp_len = sizeof(struct sockaddr_storage);
	int sock = -1;

	while ((sock = accept(listener_socket,
			      (struct sockaddr *)&sockaddr_tmp,
			      &sockaddr_tmp_len)) == -1) {
		const int err = errno;

		LOG_ERRNO("TCP", "accept()", err);

		// See "Error handling" section of Linux man page
		if (err != EAGAIN && err != EINTR && err != ENETDOWN &&
				err != EPROTO && err != ENOPROTOOPT &&
				err != EHOSTDOWN && err != ENONET &&
				err != EHOSTUNREACH && err != EOPNOTSUPP &&
				err != ENETUNREACH && err != EWOULDBLOCK)
			return -1;
	}

	char *const cla_addr = cla_tcp_sockaddr_to_cla_addr(
		(struct sockaddr *)&sockaddr_tmp,
		sockaddr_tmp_len
	);

	if (cla_addr == NULL) {
		close(sock);
		return -1;
	}

	LOGF_INFO("TCP: Connection accepted from %s (via CLA %s)!",
	     cla_addr, config->base.vtable->cla_name_get());

	if (addr != NULL)
		*addr = cla_addr;
	else
		free(cla_addr);

	/* Disable the nagle algorithm to prevent delays in responses */
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		       &enable, sizeof(int)) < 0) {
		LOG_ERRNO("TCP", "setsockopt(TCP_NODELAY)", errno);
	}

	return sock;
}

void cla_tcp_disconnect_handler(struct cla_link *link)
{
	struct cla_tcp_link *tcp_link = (struct cla_tcp_link *)link;

	shutdown(tcp_link->connection_socket, SHUT_RDWR);
	close(tcp_link->connection_socket);
	cla_generic_disconnect_handler(link);
}

void cla_tcp_single_disconnect_handler(struct cla_link *link)
{
	struct cla_tcp_single_config *tcp_config =
		(struct cla_tcp_single_config *)link->config;

	cla_tcp_disconnect_handler(link);
	tcp_config->link = NULL;
}

static void handle_established_connection(
	struct cla_tcp_single_config *config, const char *const cla_addr,
	int sock, const size_t struct_size)
{
	ASSERT(struct_size >= sizeof(struct cla_tcp_link));
	struct cla_tcp_link *link = malloc(struct_size);

	ASSERT(!config->link);
	config->link = link;

	if (cla_tcp_link_init(link, sock, &config->base, cla_addr, true)
			!= UD3TN_OK) {
		LOG_ERROR("TCP: Error creating a link instance!");
	} else {
		// Notify the BP task of the newly established connection...
		const struct bundle_agent_interface *bundle_agent_interface =
			config->base.base.bundle_agent_interface;

		bundle_processor_inform(
			bundle_agent_interface->bundle_signaling_queue,
			(struct bundle_processor_signal) {
				.type = BP_SIGNAL_NEW_LINK_ESTABLISHED,
				.peer_cla_addr = cla_get_cla_addr_from_link(
					&link->base
				),
			}
		);

		cla_link_wait_cleanup(&link->base);
	}
	config->link = NULL;
	free(link);
}

void cla_tcp_single_connect_task(struct cla_tcp_single_config *config,
				 const size_t struct_size)
{
	for (;;) {
		LOGF_INFO(
			"TCP: CLA \"%s\": Attempting to connect to \"%s:%s\".",
			config->base.base.vtable->cla_name_get(),
			config->node,
			config->service
		);

		if (cla_tcp_rate_limit_connection_attempts(&config->rl_config))
			break;
		if (cla_tcp_connect(&config->base,
				    config->node, config->service) != UD3TN_OK) {
			LOGF_WARN(
				"TCP: CLA \"%s\": Connection failed, will retry as long as a contact is ongoing.",
				config->base.base.vtable->cla_name_get()
			);
		} else {
			handle_established_connection(config, NULL,
						      config->base.socket,
						      struct_size);
			LOGF_WARN(
				"TCP: CLA \"%s\": Connection terminated, will reconnect as soon as a contact occurs.",
				config->base.base.vtable->cla_name_get()
			);
		}

		// Wait until _some_ contact starts.
		hal_semaphore_take_blocking(config->contact_activity_sem);
		hal_semaphore_release(config->contact_activity_sem);
	}
}

void cla_tcp_single_listen_task(struct cla_tcp_single_config *config,
				const size_t struct_size)
{
	for (;;) {
		const int sock = cla_tcp_accept_from_socket(
			&config->base,
			config->base.socket,
			NULL
		);

		if (sock == -1)
			break; // cla_tcp_accept_from_socket failing is fatal

		handle_established_connection(config, NULL, sock, struct_size);

		LOGF_DEBUG(
			"TCP: CLA \"%s\" is looking for a new connection now!",
			config->base.base.vtable->cla_name_get()
		);
	}

	LOG_ERROR("TCP: Socket connection broke, terminating listener.");
	abort();
}

void cla_tcp_single_link_creation_task(struct cla_tcp_single_config *config,
				       const size_t struct_size)
{
	if (config->tcp_active) {
		cla_tcp_single_connect_task(config, struct_size);
	} else {
		if (cla_tcp_listen(&config->base,
				   config->node, config->service,
				   CLA_TCP_SINGLE_BACKLOG) != UD3TN_OK) {
			LOGF_ERROR(
				"TCP: CLA \"%s\" failed to bind to \"%s:%s\".",
				config->base.base.vtable->cla_name_get(),
				config->node,
				config->service
			);
		} else {
			cla_tcp_single_listen_task(config, struct_size);
		}
	}

	LOGF_ERROR(
		"TCP: CLA \"%s\" link task terminated.",
		config->base.base.vtable->cla_name_get()
	);
	if (CLA_TCP_ABORT_ON_LINK_TASK_TERMINATION)
		abort();
}

int cla_tcp_rate_limit_connection_attempts(
	struct cla_tcp_rate_limit_config *config)
{
	const uint64_t rt_limit_ms = CLA_TCP_RETRY_INTERVAL_MS;
	const int rt_max_attempts = CLA_TCP_MAX_RETRY_ATTEMPTS;
	const uint64_t now_ms = hal_time_get_timestamp_ms();

	if (config->last_connection_attempt_ms + rt_limit_ms > now_ms) {
		if (rt_max_attempts <= 0) {
			LOGF_INFO(
				"TCP: Last connection attempt was less than %llu ms ago, delaying next attempt.",
				rt_limit_ms
			);
		} else {
			if (config->last_connection_attempt_no >=
					rt_max_attempts) {
				LOGF_INFO(
					"TCP: Final retry %d of %d failed.",
					config->last_connection_attempt_no,
					rt_max_attempts
				);
				return -1;
			}
			LOGF_INFO(
				"TCP: Last connection attempt %d of %d was less than %llu ms ago, delaying next attempt.",
				config->last_connection_attempt_no,
				rt_max_attempts,
				rt_limit_ms
			);
			config->last_connection_attempt_no++;
		}
		hal_task_delay(rt_limit_ms);
		config->last_connection_attempt_ms = (
			hal_time_get_timestamp_ms()
		);
	} else {
		config->last_connection_attempt_ms = now_ms;
		config->last_connection_attempt_no = 1;
	}
	return 0;
}

struct cla_tx_queue cla_tcp_single_get_tx_queue(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	// For single-connection CLAs, these parameters are unused...
	(void)eid;
	(void)cla_addr;

	struct cla_tcp_link *const link =
		((struct cla_tcp_single_config *)config)->link;

	// No active link!
	if (!link)
		return (struct cla_tx_queue){ NULL, NULL };

	hal_semaphore_take_blocking(link->base.tx_queue_sem);

	// Freed while trying to obtain it
	if (!link->base.tx_queue_handle)
		return (struct cla_tx_queue){ NULL, NULL };

	return (struct cla_tx_queue){
		.tx_queue_handle = link->base.tx_queue_handle,
		.tx_queue_sem = link->base.tx_queue_sem,
	};
}

enum cla_link_update_result cla_tcp_single_start_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	struct cla_tcp_single_config *tcp_config =
		(struct cla_tcp_single_config *)config;

	// If we are the first contact (in parallel), start the connection!
	if (tcp_config->num_active_contacts == 0)
		hal_semaphore_release(tcp_config->contact_activity_sem);
	tcp_config->num_active_contacts++;

	// UNUSED
	(void)eid;
	(void)cla_addr;

	if (tcp_config->link)
		return CLA_LINK_UPDATE_PERFORMED;
	else
		return CLA_LINK_UPDATE_INITIATED;
}

enum cla_link_update_result cla_tcp_single_end_scheduled_contact(
	struct cla_config *config, const char *eid, const char *cla_addr)
{
	struct cla_tcp_single_config *tcp_config =
		(struct cla_tcp_single_config *)config;

	tcp_config->num_active_contacts--;
	// Block the link creation task from retrying.
	if (tcp_config->num_active_contacts == 0)
		hal_semaphore_take_blocking(tcp_config->contact_activity_sem);

	// UNUSED
	(void)eid;
	(void)cla_addr;

	if (!tcp_config->link)
		return CLA_LINK_UPDATE_PERFORMED;
	else
		return CLA_LINK_UPDATE_INITIATED;
}

enum ud3tn_result parse_tcp_active(const char *str, bool *tcp_active)
{
	if (!strcmp(str, CLA_OPTION_TCP_ACTIVE))
		*tcp_active = true;
	else if (!strcmp(str, CLA_OPTION_TCP_PASSIVE))
		*tcp_active = false;
	else
		return UD3TN_FAIL;

	return UD3TN_OK;
}
