#ifndef TARANTOOL_BOX_OPT_DEF_H_INCLUDED
#define TARANTOOL_BOX_OPT_DEF_H_INCLUDED
/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "trivia/util.h"
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

enum opt_type {
	OPT_BOOL,	/* bool */
	OPT_UINT32,	/* uint32_t */
	OPT_INT64,	/* int64_t */
	OPT_FLOAT,	/* double */
	OPT_STR,	/* char[] */
	OPT_STRPTR,	/* char*  */
	OPT_ENUM,	/* enum */
	opt_type_MAX,
};

extern const char *opt_type_strs[];

typedef int64_t (*opt_def_to_enum_cb)(const char *str, uint32_t len);

struct opt_def {
	const char *name;
	enum opt_type type;
	ptrdiff_t offset;
	uint32_t len;

	const char *enum_name;
	int enum_size;
	const char **enum_strs;
	uint32_t enum_max;
	/** If not NULL, used to get a enum value by a string. */
	opt_def_to_enum_cb to_enum;
};

#define OPT_DEF(key, type, opts, field) \
	{ key, type, offsetof(opts, field), sizeof(((opts *)0)->field), \
	  NULL, 0, NULL, 0, NULL }

#define OPT_DEF_ENUM(key, enum_name, opts, field, to_enum) \
	{ key, OPT_ENUM, offsetof(opts, field), sizeof(int), #enum_name, \
	  sizeof(enum enum_name), enum_name##_strs, enum_name##_MAX, to_enum }

#define OPT_END {NULL, opt_type_MAX, 0, 0, NULL, 0, NULL, 0, NULL}

struct region;

/**
 * Populate key options from their msgpack-encoded representation
 * (msgpack map).
 */
int
opts_decode(void *opts, const struct opt_def *reg, const char **map,
	    uint32_t errcode, uint32_t field_no, struct region *region);

/**
 * Populate one options from msgpack-encoded representation
 */
int
opts_parse_key(void *opts, const struct opt_def *reg, const char *key,
	       uint32_t key_len, const char **data, uint32_t errcode,
	       uint32_t field_no, struct region *region);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* TARANTOOL_BOX_OPT_DEF_H_INCLUDED */
