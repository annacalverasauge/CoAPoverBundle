// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "bundle6/sdnv.h"

/* 0b01111111 */
#define SDNV_VALUE_MASK  0x7F
/* 0b10000000 */
#define SDNV_MARKER_MASK 0x80

void sdnv_reset(struct sdnv_state *state)
{
	state->status = SDNV_IN_PROGRESS;
	state->error  = SDNV_ERROR_NONE;
	state->bytes_parsed = 0;
}

static inline int sdnv_validate_byte_u32
	(const struct sdnv_state *state, const uint32_t *value)
{
	/* returns true if the byte can fit into the already parsed value. */
	/* This is independent of the value itself, but dependent on its */
	/* length. Up to four bytes definitely fit; if we get a fifth byte, */
	/* the value has to be checked whether it contains bits that would */
	/* ´fall out´ when left-shifting by 7... */
	/* a SDNV for the max. u32 0xFFFFFFFF would be 0x8FFFFFFF7F and */
	/* after reading 4 bits the *value would be equal to 0x1FFFFFF */
	return (
		(state->bytes_parsed < 4) ||
		(state->bytes_parsed == 4 && *value <= 0x1FFFFFF)
	);
}

static inline int sdnv_validate_byte_u8
	(const struct sdnv_state *state, const uint8_t *value)
{
	/* max. SDNV: 0x817F */
	return (
		(state->bytes_parsed < 1) ||
		(state->bytes_parsed == 1 && *value <= 0x1)
	);
}

static inline int sdnv_validate_byte_u16
	(const struct sdnv_state *state, const uint16_t *value)
{
	/* max. SDNV: 0x83FF7F */
	return (
		(state->bytes_parsed < 2) ||
		(state->bytes_parsed == 2 && *value <= 0x1FF)
	);
}

static inline int sdnv_validate_byte_u64
	(const struct sdnv_state *state, const uint64_t *value)
{
	/* max. SDNV: 0x81FFFFFFFFFFFFFFFF7F */
	return (
		(state->bytes_parsed < 9) ||
		(state->bytes_parsed == 9 && *value <= 0x1FFFFFFFFFFFFFF)
	);
}

#define sdnv_read_generic(state, value, byte, validate_function) \
do { \
	__typeof__(state) _state = (state); \
	__typeof__(value) _value = (value); \
	__typeof__(byte) _byte = (byte); \
	if (!(validate_function)(_state, _value)) { \
		_state->error  = SDNV_ERROR_OVERFLOW; \
		_state->status = SDNV_ERROR; \
	} else if (_state->status == SDNV_DONE) { \
		_state->error  = SDNV_ERROR_ALREADY_DONE; \
		_state->status = SDNV_ERROR; \
	} else { \
		if (_state->bytes_parsed == 0) \
			*_value = 0; \
		else \
			*_value <<= 7; \
		*_value |= (_byte & SDNV_VALUE_MASK); \
		if (!(_byte & SDNV_MARKER_MASK)) \
			_state->status = SDNV_DONE; \
		++_state->bytes_parsed; \
	} \
} while (0)

void sdnv_read_u8(struct sdnv_state *state, uint8_t *value, uint8_t byte)
{
	sdnv_read_generic(state, value, byte, sdnv_validate_byte_u8);
}

void sdnv_read_u16(struct sdnv_state *state, uint16_t *value, uint8_t byte)
{
	sdnv_read_generic(state, value, byte, sdnv_validate_byte_u16);
}

void sdnv_read_u32(struct sdnv_state *state, uint32_t *value, uint8_t byte)
{
	sdnv_read_generic(state, value, byte, sdnv_validate_byte_u32);
}

void sdnv_read_u64(struct sdnv_state *state, uint64_t *value, uint8_t byte)
{
	sdnv_read_generic(state, value, byte, sdnv_validate_byte_u64);
}

int_fast8_t sdnv_get_size_u8(uint8_t value)
{
	if ((value & 0x80) == 0)
		return 1;
	else
		return 2;
}

int_fast8_t sdnv_get_size_u16(uint16_t value)
{
	if      ((value & 0xFF80) == 0)
		return 1;
	else if ((value & 0xC000) == 0)
		return 2;
	else
		return 3;
}

int_fast8_t sdnv_get_size_u32(uint32_t value)
{
	if      ((value & 0xFFFFFF80) == 0)
		return 1;
	else if ((value & 0xFFFFC000) == 0)
		return 2;
	else if ((value & 0xFFE00000) == 0)
		return 3;
	else if ((value & 0xF0000000) == 0)
		return 4;
	else
		return 5;
}

int_fast8_t sdnv_get_size_u64(uint64_t value)
{
	if      ((value & 0xFFFFFFFFFFFFFF80) == 0)
		return 1;
	else if ((value & 0xFFFFFFFFFFFFC000) == 0)
		return 2;
	else if ((value & 0xFFFFFFFFFFE00000) == 0)
		return 3;
	else if ((value & 0xFFFFFFFFF0000000) == 0)
		return 4;
	else if ((value & 0xFFFFFFF800000000) == 0)
		return 5;
	else if ((value & 0xFFFFFC0000000000) == 0)
		return 6;
	else if ((value & 0xFFFE000000000000) == 0)
		return 7;
	else if ((value & 0xFF00000000000000) == 0)
		return 8;
	else if ((value & 0x8000000000000000) == 0)
		return 9;
	else
		return 10;
}

#define sdnv_write_generic(sdnv_bytes, buffer, value) \
do { \
	__typeof__(sdnv_bytes) _sdnv_bytes = (sdnv_bytes); \
	__typeof__(buffer) _buffer = (buffer); \
	__typeof__(value) _value = (value); \
	uint8_t *_last_byte = _buffer + _sdnv_bytes - 1; \
	for (uint8_t *_b = _last_byte; _b >= _buffer; _b--) { \
		(*_b) = (_value & SDNV_VALUE_MASK) | SDNV_MARKER_MASK; \
		_value >>= 7; \
	} \
	(*_last_byte) &= SDNV_VALUE_MASK; \
} while (0)

int_fast8_t sdnv_write_u8(uint8_t *buffer, uint8_t value)
{
	int_fast8_t sdnv_bytes = sdnv_get_size_u8(value);

	sdnv_write_generic(sdnv_bytes, buffer, value);
	return sdnv_bytes;
}

int_fast8_t sdnv_write_u16(uint8_t *buffer, uint16_t value)
{
	int_fast8_t sdnv_bytes = sdnv_get_size_u16(value);

	sdnv_write_generic(sdnv_bytes, buffer, value);
	return sdnv_bytes;
}

int_fast8_t sdnv_write_u32(uint8_t *buffer, uint32_t value)
{
	int_fast8_t sdnv_bytes = sdnv_get_size_u32(value);

	sdnv_write_generic(sdnv_bytes, buffer, value);
	return sdnv_bytes;
}

int_fast8_t sdnv_write_u64(uint8_t *buffer, uint64_t value)
{
	int_fast8_t sdnv_bytes = sdnv_get_size_u64(value);

	sdnv_write_generic(sdnv_bytes, buffer, value);
	return sdnv_bytes;
}
