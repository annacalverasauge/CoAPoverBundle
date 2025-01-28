// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "ud3tn/bundle.h"
#include "ud3tn/common.h"

// RFC 5050
#include "bundle6/bundle6.h"
#include "bundle6/serializer.h"

// BPv7-bis
#include "bundle7/bundle7.h"
#include "bundle7/bundle_age.h"
#include "bundle7/eid.h"
#include "bundle7/serializer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


static inline void bundle_reset_internal(struct bundle *bundle)
{
	if (bundle == NULL)
		return;
	bundle->protocol_version = 0x06;
	bundle->proc_flags = BUNDLE_FLAG_NONE;
	bundle->ret_constraints = BUNDLE_RET_CONSTRAINT_NONE;

	// EIDs
	bundle->destination = NULL;
	bundle->source = NULL;
	bundle->report_to = NULL;
	bundle->current_custodian = NULL;

	bundle->crc_type = DEFAULT_BPV7_CRC_TYPE;
	bundle->creation_timestamp_ms = 0;
	bundle->reception_timestamp_ms = 0;
	bundle->sequence_number = 0;
	bundle->lifetime_ms = 0;
	bundle->fragment_offset = 0;
	bundle->total_adu_length = 0;
	bundle->primary_block_length = 0;
	bundle->blocks = NULL;
	bundle->payload_block = NULL;

	bundle->forwarding_refcount = 0;
	bundle->forwarding_failed = false;
	bundle->parent = NULL;
	bundle->bdm_auth_validated = false;
}

struct bundle *bundle_init(void)
{
	struct bundle *bundle;

	bundle = malloc(sizeof(struct bundle));
	bundle_reset_internal(bundle);
	return bundle;
}

inline void bundle_free_dynamic_parts(struct bundle *bundle)
{
	if (!bundle)
		return;

	// EIDs
	free(bundle->destination);
	free(bundle->source);
	free(bundle->report_to);
	free(bundle->current_custodian);

	while (bundle->blocks != NULL)
		bundle->blocks = bundle_block_entry_free(bundle->blocks);
}

void bundle_reset(struct bundle *bundle)
{
	bundle_free_dynamic_parts(bundle);
	bundle_reset_internal(bundle);
}

void bundle_free(struct bundle *bundle)
{
	if (bundle == NULL)
		return;
	bundle_free_dynamic_parts(bundle);
	free(bundle);
}

void bundle_drop(struct bundle *bundle)
{
	ASSERT(bundle->ret_constraints == BUNDLE_RET_CONSTRAINT_NONE);
	bundle_free(bundle);
}

void bundle_copy_headers(struct bundle *to, const struct bundle *from)
{
	memcpy(to, from, sizeof(struct bundle));

	// Increase EID reference counters
	if (to->destination != NULL)
		to->destination = strdup(to->destination);
	if (to->source != NULL)
		to->source = strdup(to->source);
	if (to->report_to != NULL)
		to->report_to = strdup(to->report_to);
	if (to->current_custodian != NULL)
		to->current_custodian = strdup(to->current_custodian);

	// No extension blocks are copied
	to->blocks = NULL;
	to->payload_block = NULL;
}

enum ud3tn_result bundle_recalculate_header_length(struct bundle *bundle)
{
	switch (bundle->protocol_version) {
	// RFC 5050
	case 6:
		bundle6_recalculate_header_length(bundle);
		break;
	// BPv7
	case 7:
		bundle7_recalculate_primary_block_length(bundle);
		break;
	default:
		return UD3TN_FAIL;
	}
	return UD3TN_OK;
}


struct bundle *bundle_dup(const struct bundle *bundle)
{
	struct bundle *dup;
	struct bundle_block_list *cur_block;

	if (!bundle)
		return NULL;

	dup = malloc(sizeof(struct bundle));
	if (dup == NULL)
		return NULL;
	memcpy(dup, bundle, sizeof(struct bundle));

	// Allocate new EID references
	if (dup->source)
		dup->source = strdup(dup->source);
	if (dup->destination)
		dup->destination = strdup(dup->destination);
	if (dup->report_to)
		dup->report_to = strdup(dup->report_to);
	if (dup->current_custodian)
		dup->current_custodian = strdup(dup->current_custodian);

	// Duplicate extension blocks
	dup->blocks = bundle_block_list_dup(bundle->blocks);
	if (bundle->blocks != NULL && dup->blocks == NULL) {
		bundle_free(dup);
		return NULL;
	}

	// Search for payload block
	cur_block = dup->blocks;
	while (cur_block != NULL) {
		// Found it!
		if (cur_block->data->type == BUNDLE_BLOCK_TYPE_PAYLOAD) {
			dup->payload_block = cur_block->data;
			break;
		}
		cur_block = cur_block->next;
	}

	return dup;
}

enum bundle_routing_priority bundle_get_routing_priority(
	const struct bundle *bundle)
{
	// If there are any retention constaints for the bundle or the bundle
	// has expedited priority (RFC 5050-only)
	if (HAS_FLAG(bundle->ret_constraints, BUNDLE_RET_CONSTRAINT_FLAG_OWN) ||
	    HAS_FLAG(bundle->ret_constraints, BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED) ||
	    (bundle->protocol_version == 6 &&
	     HAS_FLAG(bundle->proc_flags, BUNDLE_V6_FLAG_EXPEDITED_PRIORITY)))
		return BUNDLE_RPRIO_HIGH;
	// BPv7-bis bundles without retention constraints are routed with
	// normal priority
	else if (bundle->protocol_version == 7 ||
		 HAS_FLAG(bundle->proc_flags, BUNDLE_V6_FLAG_NORMAL_PRIORITY))
		return BUNDLE_RPRIO_NORMAL;
	else
		return BUNDLE_RPRIO_LOW;
}

size_t bundle_get_serialized_size(struct bundle *bundle)
{
	switch (bundle->protocol_version) {
	// RFC 5050
	case 6:
		return bundle6_get_serialized_size(bundle);
	// BPv7-bis
	case 7:
		return bundle7_get_serialized_size(bundle);
	default:
		return 0;
	}
}

struct bundle_list *bundle_list_entry_create(struct bundle *bundle)
{
	if (bundle == NULL)
		return NULL;

	struct bundle_list *entry = malloc(sizeof(struct bundle_list));

	if (entry == NULL)
		return NULL;

	entry->data = bundle;
	entry->next = NULL;

	return entry;
}

struct bundle_list *bundle_list_entry_free(struct bundle_list *entry)
{
	struct bundle_list *next;

	if (!entry)
		return NULL;
	next = entry->next;
	bundle_free(entry->data);
	free(entry);
	return next;
}

struct bundle_block *bundle_block_find_first_by_type(
	struct bundle_block_list *blocks, enum bundle_block_type type)
{
	while (blocks != NULL) {
		if (blocks->data->type == type)
			return blocks->data;
		blocks = blocks->next;
	}

	return NULL;
}


struct bundle_block *bundle_block_create(enum bundle_block_type t)
{
	struct bundle_block *block = malloc(sizeof(struct bundle_block));

	if (block == NULL)
		return NULL;
	block->type = t;
	block->number = (t == BUNDLE_BLOCK_TYPE_PAYLOAD) ? 1 : 0;
	block->flags = BUNDLE_BLOCK_FLAG_NONE;
	block->eid_refs = NULL;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = 0;
	block->data = NULL;
	return block;
}

struct bundle_block_list *bundle_block_entry_create(struct bundle_block *b)
{
	struct bundle_block_list *entry;

	if (b == NULL)
		return NULL;
	entry = malloc(sizeof(struct bundle_block_list));
	if (entry == NULL)
		return NULL;
	entry->data = b;
	entry->next = NULL;
	return entry;
}

void bundle_block_free(struct bundle_block *b)
{
	if (b != NULL) {
		if (b->eid_refs != NULL)
			free(b->eid_refs);
		if (b->data != NULL)
			free(b->data);
		free(b);
	}
}

struct bundle_block_list *bundle_block_entry_free(struct bundle_block_list *e)
{
	struct bundle_block_list *next;

	if (!e)
		return NULL;
	next = e->next;
	bundle_block_free(e->data);
	free(e);
	return next;
}

struct bundle_block *bundle_block_dup(const struct bundle_block *b)
{
	struct bundle_block *dup;

	if (!b)
		return NULL;
	dup = malloc(sizeof(struct bundle_block));
	if (dup == NULL)
		return NULL;
	memcpy(dup, b, sizeof(struct bundle_block));

	const struct endpoint_list *cur_ref = b->eid_refs;

	dup->eid_refs = NULL;
	while (cur_ref != NULL) {
		struct endpoint_list *new = malloc(
			sizeof(struct endpoint_list));

		new->eid = strdup(cur_ref->eid);
		new->next = dup->eid_refs;
		dup->eid_refs = new;
		cur_ref = cur_ref->next;
	}

	dup->data = malloc(b->length);
	if (dup->data == NULL)
		goto err;
	memcpy(dup->data, b->data, b->length);
	return dup;

err:
	bundle_block_free(dup);
	return NULL;
}

struct bundle_block_list *bundle_block_entry_dup(const struct bundle_block_list *e)
{
	struct bundle_block *dup;
	struct bundle_block_list *result;

	if (!e)
		return NULL;
	dup = bundle_block_dup(e->data);
	if (dup == NULL)
		return NULL;
	result = bundle_block_entry_create(dup);
	if (result == NULL)
		bundle_block_free(dup);
	return result;
}

struct bundle_block_list *bundle_block_list_dup(struct bundle_block_list *e)
{
	struct bundle_block_list *dup = NULL, **cur = &dup;

	while (e != NULL) {
		*cur = bundle_block_entry_dup(e);
		if (!*cur) {
			while (dup)
				dup = bundle_block_entry_free(dup);
			return NULL;
		}
		cur = &(*cur)->next;
		e = e->next;
	}
	return dup;
}

enum ud3tn_result bundle_serialize(
	struct bundle *bundle,
	enum ud3tn_result (*write)(void *cla_obj, const void *, const size_t),
	void *cla_obj)
{
	switch (bundle->protocol_version) {
	// RFC 5050
	case 6:
		return bundle6_serialize(bundle, write, cla_obj);
	// BPv7
	case 7:
		return bundle7_serialize(bundle, write, cla_obj);
	default:
		return UD3TN_FAIL;
	}
	return UD3TN_OK;
}

size_t bundle_get_first_fragment_min_size(struct bundle *bundle)
{
	switch (bundle->protocol_version) {
	// RFC 5050
	case 6:
		return bundle6_get_first_fragment_min_size(bundle);
	// BPv7
	case 7:
		return bundle7_get_first_fragment_min_size(bundle);
	default:
		return 0;
	}
}

size_t bundle_get_mid_fragment_min_size(struct bundle *bundle)
{
	switch (bundle->protocol_version) {
	// RFC 5050
	case 6:
		return bundle6_get_mid_fragment_min_size(bundle);
	// BPv7
	//
	// The payload block is always the last extension block of
	// a bundle. All extension blocks are sent with the first
	// fragment. Therefore there is no distinction between last
	// and middle fragment.
	case 7:
		return bundle7_get_last_fragment_min_size(bundle);
	default:
		return 0;
	}
}

size_t bundle_get_last_fragment_min_size(struct bundle *bundle)
{
	switch (bundle->protocol_version) {
	// RFC 5050
	case 6:
		return bundle6_get_last_fragment_min_size(bundle);
	// BPv7
	case 7:
		return bundle7_get_last_fragment_min_size(bundle);
	default:
		return 0;
	}
}

uint64_t bundle_get_expiration_time_ms(const struct bundle *const bundle)
{
	if (bundle->creation_timestamp_ms != 0)
		return bundle->creation_timestamp_ms + bundle->lifetime_ms;

	const struct bundle_block *const age_block = bundle_block_find_first_by_type(
		bundle->blocks, BUNDLE_BLOCK_TYPE_BUNDLE_AGE);

	uint64_t bundle_age_ms;

	if (!age_block || !bundle_age_parse(&bundle_age_ms, age_block->data,
	    age_block->length))
		return 0;

	if (bundle->lifetime_ms <= bundle_age_ms)
		return 0;

	// EXP_TIME = CUR_TIME + REMAINING_LIFETIME
	// REMAINING_LIFETIME = TOTAL_LIFETIME - TOTAL_AGE
	// TOTAL_AGE = AGE_IN_BLOCK + LOCAL_STORAGE_DURATION
	// LOCAL_STORAGE_DURATION = CUR_TIME - RECEPTION_TIME
	// Thus: EXP_TIME = TOTAL_LIFETIME - AGE_IN_BLOCK + RECEPTION_TIME
	return (bundle->lifetime_ms - bundle_age_ms +
		bundle->reception_timestamp_ms);
}

uint64_t bundle_get_expiration_time_s(const struct bundle *const bundle)
{
	const uint64_t exp_time_ms = bundle_get_expiration_time_ms(bundle);

	if (exp_time_ms == 0)
		return 0;

	return (exp_time_ms + 500) / 1000;
}

enum ud3tn_result bundle_age_update(struct bundle *bundle,
	const uint64_t dwell_time_ms)
{
	uint64_t bundle_age;
	struct bundle_block *block = bundle_block_find_first_by_type(
		bundle->blocks, BUNDLE_BLOCK_TYPE_BUNDLE_AGE);

	// No Age Block found that needs to be updated
	if (block == NULL)
		return UD3TN_OK;

	if (!bundle_age_parse(&bundle_age, block->data, block->length))
		return UD3TN_FAIL;

	bundle_age += dwell_time_ms;

	uint8_t *buffer = malloc(BUNDLE_AGE_MAX_ENCODED_SIZE);

	// Out of memory
	if (buffer == NULL)
		return UD3TN_FAIL;

	free(block->data);
	block->data = buffer;
	block->length = bundle_age_serialize(bundle_age, buffer,
		BUNDLE_AGE_MAX_ENCODED_SIZE);

	return UD3TN_OK;
}

struct bundle_unique_identifier bundle_get_unique_identifier(
	const struct bundle *bundle)
{
	return (struct bundle_unique_identifier){
		.protocol_version = bundle->protocol_version,
		.source = strdup(bundle->source),
		.creation_timestamp_ms = bundle->creation_timestamp_ms,
		.sequence_number = bundle->sequence_number,
		.fragment_offset = bundle->fragment_offset,
		.payload_length = bundle->payload_block->length
	};
}

void bundle_free_unique_identifier(struct bundle_unique_identifier *id)
{
	free(id->source);
}

bool bundle_is_equal(
	const struct bundle *bundle, const struct bundle_unique_identifier *id)
{
	return (
		bundle_is_equal_parent(bundle, id) &&
		bundle->fragment_offset == id->fragment_offset &&
		bundle->payload_block->length == id->payload_length
	);
}

bool bundle_is_equal_parent(
	const struct bundle *bundle, const struct bundle_unique_identifier *id)
{
	return (
		bundle->protocol_version == id->protocol_version &&
		strcmp(bundle->source, id->source) == 0 && // XXX '==' may be ok
		bundle->creation_timestamp_ms == id->creation_timestamp_ms &&
		bundle->sequence_number == id->sequence_number
	);
}

struct bundle_adu bundle_adu_init(const struct bundle *bundle)
{
	struct bundle_adu bundle_adu;

	// Force initialize also the alignment gaps in the structure
	// so we can safely pass it via pipe queue
	memset(&bundle_adu, 0, sizeof(struct bundle_adu));

	bundle_adu.protocol_version = bundle->protocol_version;
	bundle_adu.proc_flags = bundle->proc_flags & ~BUNDLE_FLAG_IS_FRAGMENT;
	bundle_adu.source = strdup(bundle->source);
	bundle_adu.destination = strdup(bundle->destination);
	bundle_adu.payload = NULL;
	bundle_adu.length = 0;
	bundle_adu.bundle_creation_timestamp_ms = bundle->creation_timestamp_ms;
	bundle_adu.bundle_sequence_number = bundle->sequence_number;
	bundle_adu.bdm_auth_validated = bundle->bdm_auth_validated;

	return bundle_adu;
}

struct bundle_adu bundle_to_adu(struct bundle *bundle)
{
	struct bundle_adu adu = bundle_adu_init(bundle);

	adu.payload = bundle->payload_block->data;
	adu.length = bundle->payload_block->length;
	bundle->payload_block->data = NULL;
	bundle->payload_block->length = 0;
	return adu;
}

void bundle_adu_free_members(struct bundle_adu adu)
{
	free(adu.source);
	free(adu.destination);
	free(adu.payload);
}
