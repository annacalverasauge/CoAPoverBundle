// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#ifndef BUNDLE6_SERIALIZER_H_INCLUDED
#define BUNDLE6_SERIALIZER_H_INCLUDED

#include "ud3tn/bundle.h"
#include "ud3tn/result.h"

#include <stddef.h>

enum ud3tn_result bundle6_serialize(
	struct bundle *bundle,
	enum ud3tn_result (*write)(void *cla_obj, const void *, const size_t),
	void *cla_obj);

#endif /* BUNDLE6_SERIALIZER_H_INCLUDED */
