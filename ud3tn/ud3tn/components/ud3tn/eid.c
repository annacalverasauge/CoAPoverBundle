// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/common.h"
#include "ud3tn/eid.h"
#include "ud3tn/result.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// https://datatracker.ietf.org/doc/html/draft-ietf-dtn-bpbis-31#section-4.2.5.1
enum ud3tn_result validate_eid(const char *const eid)
{
	size_t len;
	const char *cur;

	switch (get_eid_scheme(eid)) {
	case EID_SCHEME_DTN:
		len = strlen(eid);
		// minimum is dtn:none or dtn://C with C being a char
		if (len < 7)
			return UD3TN_FAIL;
		else if (len == 8 && memcmp(eid, "dtn:none", 8) == 0)
			return UD3TN_OK;
		else if (memcmp(eid, "dtn://", 6))
			return UD3TN_FAIL;
		// check node-name
		cur = &eid[6]; // after "dtn://"
		while (*cur) {
			if (!isalnum(*cur) && *cur != '-' &&
			    *cur != '.' && *cur != '_')
				break;
			cur++;
		}
		// fail if node-name is zero-length
		if (cur == &eid[6])
			return UD3TN_FAIL;
		// note that we allow dtn EID with and without trailing slash
		if (*cur == '\0')
			return UD3TN_OK;
		// fail if node-name is terminated by something other than '/'
		if (*cur != '/')
			return UD3TN_FAIL;
		// check demux
		return validate_dtn_eid_demux(cur);
	case EID_SCHEME_IPN:
		return validate_ipn_eid(eid, NULL, NULL);
	default:
		return UD3TN_FAIL;
	}
}

enum ud3tn_result validate_dtn_eid_demux(const char *demux)
{
	if (!demux)
		return UD3TN_FAIL;

	while (*demux) {
		// VCHAR is in range %x21-7E -> RFC 5234)
		if (*demux < 0x21 || *demux > 0x7E)
			return UD3TN_FAIL;
		demux++;
	}
	return UD3TN_OK;
}

enum ud3tn_result validate_local_eid(const char *const eid)
{
	if (validate_eid(eid) != UD3TN_OK)
		return UD3TN_FAIL;

	uint64_t service;

	switch (get_eid_scheme(eid)) {
	case EID_SCHEME_DTN:
		// The EID must start with dtn://
		if (strlen(eid) < 7 || memcmp(eid, "dtn://", 6))
			return UD3TN_FAIL;

		const char *const slash_pos = strchr(&eid[6], '/');

		// The first-contained slash must be there...
		if (!slash_pos)
			return UD3TN_FAIL;
		// ...and it must terminate the EID.
		if (slash_pos - strlen(eid) + 1 == eid)
			return UD3TN_OK;
		return UD3TN_FAIL;
	case EID_SCHEME_IPN:
		// Service number must be zero
		if (validate_ipn_eid(eid, NULL, &service) == UD3TN_OK &&
		    service == 0)
			return UD3TN_OK;
		return UD3TN_FAIL;
	default:
		return UD3TN_FAIL;
	}
}

char *preprocess_local_eid(const char *const eid)
{
	const size_t eid_len = strlen(eid);

	switch (get_eid_scheme(eid)) {
	case EID_SCHEME_DTN:
		// Allow dtn://node.dtn -> dtn://node.dtn/
		// (Make sure a slash terminates the EID.)

		if (eid_len < 7 || memcmp(eid, "dtn://", 6))
			return NULL;

		const char *const slash_pos = strchr(&eid[6], '/');

		if (slash_pos != NULL)
			return strdup(eid);

		char *const returned_eid_dtn = malloc(eid_len + 2);

		if (returned_eid_dtn == NULL)
			return NULL;

		if (snprintf(returned_eid_dtn, eid_len + 2, "%s/", eid) !=
		    (long)eid_len + 1) {
			free(returned_eid_dtn);
			return NULL;
		}

		return returned_eid_dtn;
	case EID_SCHEME_IPN:
		// Allow ipn:1 -> ipn:1.0
		// (Make sure node and service numbers are specified.)

		if (eid_len < 5)
			return NULL;

		const char *const dot_pos = strchr(&eid[4], '.');

		if (dot_pos != NULL)
			return strdup(eid);

		char *const returned_eid_ipn = malloc(eid_len + 3);

		if (returned_eid_ipn == NULL)
			return NULL;

		if (snprintf(returned_eid_ipn, eid_len + 3, "%s.0", eid) !=
		    (long)eid_len + 2) {
			free(returned_eid_ipn);
			return NULL;
		}

		return returned_eid_ipn;
	default:
		return NULL;
	}
}

enum eid_scheme get_eid_scheme(const char *const eid)
{
	if (!eid)
		return EID_SCHEME_UNKNOWN;

	const size_t len = strlen(eid);

	if (len > EID_MAX_LEN)
		return EID_SCHEME_UNKNOWN;

	if (len >= 4 && !memcmp(eid, "dtn:", 4))
		return EID_SCHEME_DTN;
	else if (len >= 4 && !memcmp(eid, "ipn:", 4))
		return EID_SCHEME_IPN;

	return EID_SCHEME_UNKNOWN;
}

const char *parse_ipn_ull(const char *const cur, uint64_t *const out)
{
	unsigned long long tmp;
	const char *next, *next_strtoull;

	if (!cur || *cur == '\0')
		return NULL;

	// Check that only digits are part of the substring
	// Otherwise, strtoull may accept things like '-' and thousands
	// separators according to the currently set locale.
	next = cur;
	while (*next && *next >= '0' && *next <= '9')
		next++;
	if (*next != '.' && *next != '\0')
		return NULL;

	errno = 0;
	tmp = strtoull(cur, (char **)&next_strtoull, 10);

	// strtoull must have parsed the same as the digit check above
	if (next != next_strtoull)
		return NULL;
	// Special case: returns 0 -> failed OR is actually zero
	if (tmp == 0 && (next - cur != 1 || *cur != '0'))
		return NULL;
	// Special case: overflow
	if (tmp == ULLONG_MAX && errno == ERANGE)
		return NULL;

	if (out)
		*out = (uint64_t)tmp;

	return next;
}

// https://datatracker.ietf.org/doc/html/rfc6260
// check "ipn:node.service" EID
enum ud3tn_result validate_ipn_eid(
	const char *const eid,
	uint64_t *const node_out, uint64_t *const service_out)
{
	if (get_eid_scheme(eid) != EID_SCHEME_IPN)
		return UD3TN_FAIL;

	const char *cur;
	uint64_t tmp_node, tmp_service;

	cur = parse_ipn_ull(&eid[4], &tmp_node);
	if (!cur || *cur != '.')
		return UD3TN_FAIL;

	cur = parse_ipn_ull(cur + 1, &tmp_service);
	if (!cur || *cur != '\0')
		return UD3TN_FAIL;

	if (node_out)
		*node_out = tmp_node;
	if (service_out)
		*service_out = tmp_service;

	return UD3TN_OK;
}

char *get_node_id(const char *const eid)
{
	if (validate_eid(eid) != UD3TN_OK)
		return NULL;

	char *delim, *result;
	size_t eid_len;

	switch (get_eid_scheme(eid)) {
	case EID_SCHEME_DTN:
		eid_len = strlen(eid);
		// Special case, e.g., "dtn:none"
		if (eid_len < 7 || memcmp(eid, "dtn://", 6))
			return strdup(eid);
		delim = strchr((char *)&eid[6], '/');
		// There is no slash (-> EID _is_ node ID)
		if (!delim) {
			// ...but terminate it with a slash to be consistent!
			char *const terminated_eid = malloc(eid_len + 2);

			if (terminated_eid == NULL)
				return NULL;

			memcpy(terminated_eid, eid, eid_len);
			terminated_eid[eid_len] = '/';
			terminated_eid[eid_len + 1] = '\0';
			return terminated_eid;
		}
		// First-contained slash ends the EID (-> EID _is_ node ID)
		if (delim - eid_len + 1 == eid)
			return strdup(eid);
		// Demux starts with tilde (-> non-singleton EID -> no node ID)
		if (delim[1] == '~')
			return NULL;
		result = strdup(eid);
		if (result == NULL)
			return NULL;
		result[delim - eid + 1] = '\0';
		return result;
	case EID_SCHEME_IPN:
		result = strdup(eid);
		if (result == NULL)
			return NULL;
		delim = strchr(result, '.');
		ASSERT(delim);
		// No out-of-bounds access as validate_eid ensures the dot is
		// always followed by at least one digit.
		ASSERT(strlen(delim) >= 2);
		delim[1] = '0';
		delim[2] = '\0';
		return result;
	default:
		return NULL;
	}
}

const char *get_agent_id_ptr(const char *const eid)
{
	const char *delim;

	switch (get_eid_scheme(eid)) {
	case EID_SCHEME_DTN:
		// Special case, e.g., "dtn:none"
		if (strncmp(eid, "dtn://", 6))
			return NULL;
		delim = strchr((char *)&eid[6], '/');
		if (!delim || delim[1] == '\0')
			return NULL;
		return &delim[1];
	case EID_SCHEME_IPN:
		delim = strchr(eid, '.');
		if (!delim || delim[1] == '\0')
			return NULL;
		return &delim[1];
	default:
		return NULL;
	}
}
