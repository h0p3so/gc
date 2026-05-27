// TODO: std everything to stdint & stdbool
#ifndef GC_H
#define GC_H

#include <stdint.h>
#include <stdbool.h>

#ifndef GC_API
#	define GC_API
#endif

#ifndef GC_NUM_FLAGS
#	define GC_NUM_FLAGS 0
#endif

#define GC_FLAG_ARG_MODE_SHIFT     0x0
#define GC_FLAG_ARG_TYPE_SHIFT     0x2
#define GC_FLAG_MANDATORY_SHIFT    0x5
#define GC_FLAG_ARG_NUM_BASE_SHIFT 0x6

#define GC_FLAG_NO_ARGUMENT     (0x0 << GC_FLAG_ARG_MODE_SHIFT)
#define GC_FLAG_OPT_ARGUMENT    (0x1 << GC_FLAG_ARG_MODE_SHIFT)
#define GC_FLAG_MUST_ARGUMENT   (0x2 << GC_FLAG_ARG_MODE_SHIFT)

/* no bool type is defined since it can be interpreted as:
 * if the flag exists, then it is true, otherwise it is false.
 *
 * for example, --verbose is a boolean flag, whenever the user
 * gives it, it is taken as true, it wouldn't make sense having
 * something like --verbose=false
 */
#define GC_ARG_TYPE_TEXT        (0x0 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_ARG_TYPE_DOUB        (0x1 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_ARG_TYPE_UI32        (0x2 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_ARG_TYPE_I32         (0x3 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_ARG_TYPE_UI64        (0x4 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_ARG_TYPE_I64         (0x5 << GC_FLAG_ARG_TYPE_SHIFT)

#define GC_FLAG_IS_MANDATORY    (0x1 << GC_FLAG_MANDATORY_SHIFT)
#define GC_FLAG_ISNT_MANDATORY  (0x0 << GC_FLAG_MANDATORY_SHIFT)

/* only two bases are allowed, hexadecimal if the number is too big or decimal
 * since is the most used. However, the programmer can always change this to
 * their convenience.
 */
#define GC_FLAG_ARG_NUM_BASE_10 (0x0 << GC_FLAG_ARG_NUM_BASE_SHIFT)
#define GC_FLAG_ARG_NUM_BASE_16 (0x1 << GC_FLAG_ARG_NUM_BASE_SHIFT)

typedef unsigned char getc_opts_t;

enum GCError {
	GCE_NONE = 0,
	GCE_INVALID_SHORTNAME,
	GCE_DUPLICATED_SHORTNAME,
	GCE_DUPLICATED_LONGNAME,
	GCE_MALFORMED_OPTS,

	GCE_PROGRAMMER_FAULT_ENDS,

	GCE_UNKNOWN_SHORTNAME,
	GCE_MISSING_ARGUMENT,
	GCE_NONSENSE_ARGUMENT,
	GCE_UNKNOWN_LONGNAME,
	GCE_TOO_MANY_PARGS
};

struct GCFlag {
	union {
		char    *text;
		double   doub;
		uint64_t u64;
		int64_t  i64;
		uint32_t u32;
		int32_t  i32;
	} as;
	const char *longname;
	const char shortname;
	const getc_opts_t opts;
	/* these variables shouldn't be initialized */
	unsigned char seen;
	unsigned char aset;
};

struct GCAns {
	struct {
		uint32_t pos;
		uint32_t len;
		char *src;
	} lex;
	struct {
		char **arguments;
		uint32_t total;
		uint32_t __cap;
	} pargs;
	struct GCFlag *flast;
	enum GCError err;
};

GC_API struct GCAns getc_init (int, char**, struct GCFlag*);

#ifdef GC_IMPLEMENTATION

#define GC_PARGS_FLEXIBLE
#define GC_PARGS_GROWTH_FACTOR 8

#define GC_FLAG_ARG_MODE_MASK (0x3 << GC_FLAG_ARG_MODE_SHIFT)
#define GC_FLAG_ARG_TYPE_MASK (0x7 << GC_FLAG_ARG_TYPE_SHIFT)
#define GC_FLAG_MANDATORY     (0x1 << GC_FLAG_MANDATORY_SHIFT)
#define GC_FLAG_ARG_NUM_BASE  (0x1 << GC_FLAG_ARG_NUM_BASE_SHIFT)

#define GC_TOTAL_NUM_FLAGS (26 * 2 + 10)

#define GC_BASE_HEX 16
#define GC_BASE_DEC 10

#include <stdio.h> // TODO: remove

#include <ctype.h>
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
	for (ans->lex.pos = 1; ans->lex.pos < ans->lex.len; ans->lex.pos++) {
		const char shortname = ans->lex.src[ans->lex.pos];
		const uint32_t normal = normalize_shortname(shortname);

		if (map->s[normal] == NULL) {
			return GCE_UNKNOWN_SHORTNAME;
		}
		struct GCFlag *flag = map->s[normal];

		ans->flast = flag;
		ans->flast->seen = 1;

		const uint8_t takesarg = ((flag->opts & GC_FLAG_ARG_MODE_MASK) == GC_FLAG_MUST_ARGUMENT);
		if (takesarg && (ans->lex.pos + 1 < ans->lex.len)) {
			return GCE_MISSING_ARGUMENT;
		}

		printf("shortname: %c\n", shortname);
	}
	return GCE_NONE;
}

static enum GCError parse_argument (struct GCAns *ans) {
	if (ans->flast == NULL || ((ans->flast->opts & GC_FLAG_ARG_MODE_MASK) == GC_FLAG_NO_ARGUMENT)) {
		return GCE_NONSENSE_ARGUMENT;
	}

	char *source = ans->lex.src;
	const uint32_t base = ((ans->flast->opts & GC_FLAG_ARG_NUM_BASE) == 1) ? GC_BASE_HEX : GC_BASE_DEC;

	switch (ans->flast->opts & GC_FLAG_ARG_TYPE_MASK) {
		case GC_ARG_TYPE_TEXT: { ans->flast->as.text = source;                       break; }
		case GC_ARG_TYPE_DOUB: { ans->flast->as.doub = strtod(source, NULL);         break; }
		case GC_ARG_TYPE_UI32: { ans->flast->as.u32  = strtoul(source, NULL, base);  break; }
		case GC_ARG_TYPE_I32 : { ans->flast->as.i32  = strtol(source, NULL, base);   break; }
		case GC_ARG_TYPE_UI64: { ans->flast->as.u64  = strtoull(source, NULL, base); break; }
		case GC_ARG_TYPE_I64 : { ans->flast->as.i64  = strtoll(source, NULL, base);  break; }
		default: {
			return GCE_MALFORMED_OPTS;
		}
	}

	printf("argument: %s\n", source);
	ans->flast->aset = 1;
	return GCE_NONE;
}

static struct GCFlag *find_flag_by_longname (const char *longname, const size_t length, const struct GCMap *const map) {
	const char id = *longname;
	const uint32_t normal = normalize_shortname(id);

	struct GCFlag *flag = map->s[normal];
	if (flag != NULL) {
		const size_t longitud = strlen(flag->longname);
		if (longitud == length && (strncmp(flag->longname, longname, length) == 0)) {
			return flag;
		}
	}

	for (uint32_t i = 0; i < GC_NUM_FLAGS; i++) {
		flag = &map->l[i];
		const size_t longitud = strlen(flag->longname);
		
		if (longitud == length && (strncmp(flag->longname, longname, length) == 0)) {
			return flag;
		}
	}
	return NULL;
}

static enum GCError parse_longopt (struct GCMap *const map, struct GCAns *const ans) {
	const char *eq = strchr(ans->lex.src, '=');
	struct GCFlag *flag = NULL;

	if (eq == NULL) {
		flag = find_flag_by_longname(ans->lex.src + 2, ans->lex.len - 2, map);
	} else {
		const size_t length = ((size_t) (eq -  ans->lex.src)) - 2;
		flag = find_flag_by_longname(ans->lex.src + 2, length, map);
	}

	if (flag == NULL) {
		return GCE_UNKNOWN_LONGNAME;
	}

	printf("longname: %s\n", flag->longname);

	ans->flast = flag;
	ans->flast->seen = 1;
	return GCE_NONE;
}

// TODO validate ptr
static enum GCError parse_positional_argument (struct GCAns *ans) {
	if (ans->pargs.arguments == NULL) {
		ans->pargs.arguments = (char**) calloc(GC_PARGS_GROWTH_FACTOR, sizeof(*ans->pargs.arguments));
		ans->pargs.__cap = GC_PARGS_GROWTH_FACTOR;
		ans->pargs.total = 0;
	}

	if (ans->pargs.total == ans->pargs.__cap) {
#ifdef GC_PARGS_FLEXIBLE
		ans->pargs.__cap += GC_PARGS_GROWTH_FACTOR;
		ans->pargs.arguments = (char**) realloc(ans->pargs.arguments, ans->pargs.__cap);
#else
		return GCE_TOO_MANY_PARGS;
#endif
	}
	ans->pargs.arguments[ans->pargs.total++] = ans->lex.src;
	return GCE_NONE;
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
		ans.lex.len = strlen(ans.lex.src);

		if (*ans.lex.src == '-' && ans.lex.len >= 2 && isalnum(ans.lex.src[1])) {
			ans.err = work_short(&map, &ans);
		}
		else if (ans.lex.len >= 3 && *ans.lex.src == '-' && ans.lex.src[1] == '-' && isalnum(ans.lex.src[2])) {
			ans.err = parse_longopt(&map, &ans);
		}
		else if (ans.lex.len == 2 && *ans.lex.src == '-' && ans.lex.src[1] == '-') {
			ans.err = parse_positional_argument(&ans);
			
		} else {
			ans.err = parse_argument(&ans);
		}
	}
	return ans;
}

#endif
#endif
