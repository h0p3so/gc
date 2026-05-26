#ifndef GC_H
#define GC_H

#ifndef GC_API
#	define GC_API
#endif

#ifndef GC_NUM_FLAGS
#	define GC_NUM_FLAGS 0
#endif

#define GC_FLAG_ARG_MODE_SHIFT  0x0
#define GC_FLAG_ARG_TYPE_SHIFT  0x2
#define GC_FLAG_MANDATORY_SHIFT 0x5

#define GC_FLAG_NON_ARGUMENT   (0x0 << GC_FLAG_ARG_MODE_SHIFT)
#define GC_FLAG_MAY_ARGUMENT   (0x1 << GC_FLAG_ARG_MODE_SHIFT)
#define GC_FLAG_YES_ARGUMENT   (0x2 << GC_FLAG_ARG_MODE_SHIFT)

#define GC_ARG_TYPE_IS_TEXT    (0x0 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_ARG_TYPE_IS_DOUB    (0x1 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_ARG_TYPE_IS_UI32    (0x2 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_ARG_TYPE_IS_I32     (0x3 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_ARG_TYPE_IS_UI64    (0x4 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_ARG_TYPE_IS_I64     (0x5 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_ARG_TYPE_IS_BOOL    (0x6 << GC_FLAG_ARG_TYPE_SHIFT)

#define GC_FLAG_IS_MANDATORY   (0x1 << GC_FLAG_MANDATORY_SHIFT)
#define GC_FLAG_ISNT_MANDATORY (0x0 << GC_FLAG_MANDATORY_SHIFT)

typedef unsigned char getc_opts_t;

enum GCError {
	GCE_NONE = 0,
	GCE_INVALID_SHORTNAME,
	GCE_DUPLICATED_SHORTNAME,
	GCE_DUPLICATED_LONGNAME,

	GCE_PROGRAMMER_FAULT_ENDS,

	GCE_UNKNOWN_SHORTNAME,
};

struct GCFlag {
	const char *longname;
	const char shortname;
	const getc_opts_t opts;
	/* these variables shouldn't be initialized */
	unsigned char seen;
	unsigned char aset;
};

struct GCAns {
	struct {
		unsigned int pos;
		unsigned int len;
		char *src;
	} Lex;
	struct GCFlag *flast;
	enum GCError err;
};

GC_API struct GCAns getc_init (int, char**, struct GCFlag*);

#ifdef GC_IMPLEMENTATION


#define GC_FLAG_ARG_MODE_MASK (0x3 << GC_FLAG_ARG_MODE_SHIFT)
#define GC_FLAG_ARG_TYPE_MASK (0x7 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_FLAG_MANDATORY     (0x1 << GC_FLAG_MANDATORY_SHIFT)

#define GC_TOTAL_NUM_FLAGS (26 * 2 + 10)

#include <stdio.h> // TODO: remove

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct GCMap {
	struct GCFlag *l;
	struct GCFlag *s[GC_TOTAL_NUM_FLAGS];
};

static uint32_t normalize_shortname (const char shortname) {
	if (islower(shortname)) {
		return shortname - 'a';
	}
	if (isupper(shortname)) {
		return shortname - 'A';
	}
	return shortname - '0';
}

static enum GCError check_integrity (struct GCFlag *flags) {
	uint8_t flagseen[GC_TOTAL_NUM_FLAGS] = {0};

	for (uint32_t i = 0; i < GC_NUM_FLAGS; i++) {
		flags[i].seen = 0;
		flags[i].aset = 0;

		const char shortname = flags[i].shortname;
		if (isalnum(shortname) == 0) {
			return GCE_INVALID_SHORTNAME;
		}

		const uint32_t normal = normalize_shortname(shortname);
		if (flagseen[normal] != 0) {
			return GCE_DUPLICATED_SHORTNAME;
		}
		flagseen[normal] = 1;

		const char *longname = flags[i].longname;
		const uint32_t length = (longname) ? ((uint32_t) strlen(longname)) : 0;

		if (length == 0) {
			continue;
		}
		for (uint32_t j = i + 1; j < GC_NUM_FLAGS; j++) {
			const char *nomlong = flags[j].longname;
			if (nomlong == NULL) {
				continue;
			}
			const uint32_t longueur = (uint32_t) strlen(nomlong);
			if (longueur != length) {
				continue;
			}
			if (strncmp(nomlong, longname, length) == 0) {
				return GCE_DUPLICATED_LONGNAME;
			}
		}
	}
	return GCE_NONE;
}

static int map_flags_cmp (const void *f1, const void *f2) {
	struct GCFlag *flag1 = (struct GCFlag*) f1;
	struct GCFlag *flag2 = (struct GCFlag*) f2;

	return strcmp(flag1->longname, flag2->longname);
}

static void map_flags (struct GCMap *map, struct GCFlag *flags) {
	memset(map, 0, sizeof(*map));
	map->l = flags;
	qsort(map->l, GC_NUM_FLAGS, sizeof(struct GCFlag), map_flags_cmp);

	for (uint32_t i = 0; i < GC_NUM_FLAGS; i++) {
		const char shortname = map->l[i].shortname;
		const uint32_t normal = normalize_shortname(shortname);
		map->s[normal] = &map->l[i];
	}
}

static enum GCError work_short (struct GCMap *map, struct GCAns *ans) {
	for (ans.lex.pos = 1; ans.lex.pos < ans.lex.len; ans.lex.pos++) {
		const char shortname = arge[ans.lex.pos];
		const uint32_t normal = normalize_shortname(shortname);

		if (map->s[normal] == NULL) {
			return GCE_UNKNOWN_SHORTNAME;
		}
	}
}

GC_API struct GCAns getc_init (int argc, char **argv, struct GCFlag *flags) {
	struct GCAns ans;
	memset(&ans, 0, sizeof(ans));

	ans.err = check_integrity(flags);
	if (ans.err != GCE_NONE) {
		return ans;
	}
	if (argc == 0 || argv == NULL) {
		return ans;
	}

	struct GCMap map;
	map_flags(&map, flags);

	for (int i = 1; (i < argc) && (ans.err == GCE_NONE); i++) {
		ans.lex.src = argv[i];
		ans.lex.pos = 0;
		ans.lex.len = strlen(ans.lex.field);

		if (*ans.lex.src == '-' && ans.lex.len >= 2 && isalnum(ans.lex.src[1])) {
			ans.err = work_short(&map, arge, length);
		}
	}

	return ans;
}

#endif
#endif
