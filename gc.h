#ifndef GC_H
#define GC_H

#include <stdint.h>
#include <stdbool.h>

#ifndef GC_API
#	define GC_API static
#endif

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
	GCE_TOO_MANY_PARGS,
GCE_USER_FAULT_ENDS,
	GCE_NO_HEAP_SPACE
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
	bool seen;
	bool aset;
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

GC_API struct GCAns gc_init (uint32_t, char**, struct GCFlag*, const uint32_t, const char*);
GC_API void gc_free (struct GCAns*);

#ifdef GC_IMPLEMENTATION

#define GC_INTERNAL_ARG_MODE_SHIFT           0x0
#define GC_INTERNAL_ARG_TYPE_SHIFT           0x2
#define GC_INTERNAL_ARG_NUM_BASE_SHIFT       0x5

#define GC_INTERNAL_ARG_MODE_MASK            (0x3 << GC_INTERNAL_ARG_MODE_SHIFT)
#define GC_INTERNAL_ARG_TYPE_MASK            (0x7 << GC_INTERNAL_ARG_TYPE_SHIFT)
#define GC_INTERNAL_ARG_NUM_BASE_MASK        (0x1 << GC_INTERNAL_ARG_NUM_BASE_SHIFT)

#define GC_INTERNAL_PARGS_STEP_FACTOR        0x8
#define GC_INTERNAL_TOTAL_NUM_FLAGS          (26 * 2 + 10)

#define GC_INTERNAL_BASE_HEX                 0x10
#define GC_INTERNAL_BASE_DEC                 0x0a

#define GC_OPTS_FLAG_NON_ARG (0x0 << GC_INTERNAL_ARG_MODE_SHIFT)
#define GC_OPTS_FLAG_OPT_ARG (0x1 << GC_INTERNAL_ARG_MODE_SHIFT)
#define GC_OPTS_FLAG_REQ_ARG (0x2 << GC_INTERNAL_ARG_MODE_SHIFT)

/* no bool type is defined since it can be interpreted as:
 * if the flag exists, then it is true, otherwise it is false.
 *
 * for example, --verbose is a boolean flag, whenever the user
 * provides it, it is taken as true, it wouldn't make sense having
 * something like --verbose=false
 */
#define GC_OPTS_FLAG_ARG_TYPE_TEXT (0x0 << GC_INTERNAL_ARG_TYPE_SHIFT)
#define GC_OPTS_FLAG_ARG_TYPE_DOUB (0x1 << GC_INTERNAL_ARG_TYPE_SHIFT)
#define GC_OPTS_FLAG_ARG_TYPE_UI32 (0x2 << GC_INTERNAL_ARG_TYPE_SHIFT)
#define GC_OPTS_FLAG_ARG_TYPE_I32  (0x3 << GC_INTERNAL_ARG_TYPE_SHIFT)
#define GC_OPTS_FLAG_ARG_TYPE_UI64 (0x4 << GC_INTERNAL_ARG_TYPE_SHIFT)
#define GC_OPTS_FLAG_ARG_TYPE_I64  (0x5 << GC_INTERNAL_ARG_TYPE_SHIFT)

/* only two bases are allowed, hexadecimal if the number is too big or decimal
 * since it is the standard. However, the programmer can always change this to
 * their own convenience.
 */
#define GC_OPTS_ARG_NUM_BASE_10 (0x0 << GC_INTERNAL_ARG_NUM_BASE_SHIFT)
#define GC_OPTS_ARG_NUM_BASE_16 (0x1 << GC_INTERNAL_ARG_NUM_BASE_SHIFT)

#define GC_IS_PROGRAMMER_FAULT(err) ((err < GCE_PROGRAMMER_FAULT_ENDS) && (err != GCE_NONE))
#define GC_IS_USER_FAULT(err)       ((err > GCE_PROGRAMMER_FAULT_ENDS) && (err < GCE_USER_FAULT_ENDS))
#define GC_IS_SYS_FAULT(err)        (err > GCE_USER_FAULT_ENDS)

/* these two macros are optional:
 *
 * GC_PARGS_FLEXIBLE: allows GC to grow the array of positional arguments in case it reaches
 * the limit (GC_INTERNAL_PARGS_STEP_FACTOR).
 *
 * GC_ALLOW_GC_HANDLE_ERRORS: allows GC to handle the error messages, if this macro is not
 * set, it is responsability of the programmer to display the proper error messages.
 * */
#define GC_PARGS_FLEXIBLE
#define GC_ALLOW_GC_HANDLE_ERRORS

#ifdef GC_ALLOW_GC_HANDLE_ERRORS
#	include <stdio.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

struct GCMap {
	struct GCFlag *flags;
	struct GCFlag *mapper[GC_INTERNAL_TOTAL_NUM_FLAGS];
	const uint32_t nflags;
};

static uint32_t gc_internal_normalize_shortname (const char shortname) {
	if (islower(shortname)) {
		return shortname - 'a';
	}
	if (isupper(shortname)) {
		return shortname - 'A';
	}
	return shortname - '0';
}

static enum GCError gc_internal_check_integrity (struct GCFlag *flags, const uint32_t nflags) {
	bool flagseen[GC_INTERNAL_TOTAL_NUM_FLAGS] = {0};

	for (uint32_t i = 0; i < nflags; i++) {
		flags[i].seen = false;
		flags[i].aset = false;

		const char shortname = flags[i].shortname;
		if (isalnum(shortname) == 0) {
			return GCE_INVALID_SHORTNAME;
		}

		const uint32_t normal = gc_internal_normalize_shortname(shortname);
		if (flagseen[normal] != false) {
			return GCE_DUPLICATED_SHORTNAME;
		}
		flagseen[normal] = true;

		const char *longname = flags[i].longname;
		const uint32_t length = (longname) ? ((uint32_t) strlen(longname)) : 0;

		if (length == 0) {
			continue;
		}
		for (uint32_t j = i + 1; j < nflags; j++) {
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

static void gc_internal_map_flags (struct GCMap *map, struct GCFlag *flags) {
	map->flags = flags;

	for (uint32_t i = 0; i < map->nflags; i++) {
		const char shortname = map->flags[i].shortname;
		const uint32_t normal = gc_internal_normalize_shortname(shortname);
		map->mapper[normal] = &map->flags[i];
	}
}

static inline bool gc_internal_was_arg_set (const struct GCFlag *const flast) {
	if (flast == NULL) {
		return true;
	}
	if ((flast->opts & GC_INTERNAL_ARG_MODE_MASK) == GC_OPTS_FLAG_OPT_ARG) {
		return true;
	}
	/* IF takes an argument, argument set
	 * does NOT take an argument OR argument set
	 */
	return (((flast->opts & GC_INTERNAL_ARG_MODE_MASK) == GC_OPTS_FLAG_NON_ARG) || (flast->aset));
}

static enum GCError gc_internal_parse_shortopt (struct GCMap *map, struct GCAns *ans) {
	if (gc_internal_was_arg_set(ans->flast) == false) {
		return GCE_MISSING_ARGUMENT;
	}

	for (ans->lex.pos = 1; ans->lex.pos < ans->lex.len; ans->lex.pos++) {
		const char shortname = ans->lex.src[ans->lex.pos];
		const uint32_t normal = gc_internal_normalize_shortname(shortname);

		if (map->mapper[normal] == NULL) {
			return GCE_UNKNOWN_SHORTNAME;
		}
		struct GCFlag *flag = map->mapper[normal];

		ans->flast = flag;
		ans->flast->seen = true;
	}
	return GCE_NONE;
}

static enum GCError gc_internal_parse_argument (struct GCAns *ans) {
	if (ans->flast == NULL || ((ans->flast->opts & GC_INTERNAL_ARG_MODE_MASK) == GC_OPTS_FLAG_NON_ARG)) {
		return GCE_NONSENSE_ARGUMENT;
	}

	char *source = ans->lex.src;
	const uint32_t base = ((ans->flast->opts & GC_INTERNAL_ARG_NUM_BASE_MASK) == GC_OPTS_ARG_NUM_BASE_16) ? GC_INTERNAL_BASE_HEX : GC_INTERNAL_BASE_DEC;

	switch (ans->flast->opts & GC_INTERNAL_ARG_TYPE_MASK) {
		case GC_OPTS_FLAG_ARG_TYPE_TEXT: { ans->flast->as.text = source;                       break; }
		case GC_OPTS_FLAG_ARG_TYPE_DOUB: { ans->flast->as.doub = strtod(source, NULL);         break; }
		case GC_OPTS_FLAG_ARG_TYPE_UI32: { ans->flast->as.u32  = strtoul(source, NULL, base);  break; }
		case GC_OPTS_FLAG_ARG_TYPE_I32 : { ans->flast->as.i32  = strtol(source, NULL, base);   break; }
		case GC_OPTS_FLAG_ARG_TYPE_UI64: { ans->flast->as.u64  = strtoull(source, NULL, base); break; }
		case GC_OPTS_FLAG_ARG_TYPE_I64 : { ans->flast->as.i64  = strtoll(source, NULL, base);  break; }
		default: {
			return GCE_MALFORMED_OPTS;
		}
	}
	ans->flast->aset = true;
	return GCE_NONE;
}

static struct GCFlag *gc_internal_find_longname_flag (const char *longname, const size_t length, const struct GCMap *const map) {
	const char id = *longname;
	const uint32_t normal = gc_internal_normalize_shortname(id);

	struct GCFlag *flag = map->mapper[normal];
	if (flag != NULL && flag->longname != NULL) {
		const size_t longitud = strlen(flag->longname);
		if (longitud == length && (strncmp(flag->longname, longname, length) == 0)) {
			return flag;
		}
	}

	for (uint32_t i = 0; i < map->nflags; i++) {
		flag = &map->flags[i];
		if (flag->longname == NULL) {
			continue;
		}

		const size_t longitud = strlen(flag->longname);
		if (longitud == length && (strncmp(flag->longname, longname, length) == 0)) {
			return flag;
		}
	}
	return NULL;
}

static enum GCError gc_internal_parse_longopt (struct GCMap *const map, struct GCAns *const ans) {
	if (gc_internal_was_arg_set(ans->flast) == false) {
		return GCE_MISSING_ARGUMENT;
	}

	char *eq = strchr(ans->lex.src, '=');
	enum GCError err = GCE_NONE;

	if (eq == NULL) {
		ans->flast = gc_internal_find_longname_flag(ans->lex.src + 2, ans->lex.len - 2, map);
	} else {
		const size_t length = ((size_t) (eq -  ans->lex.src)) - 2;
		ans->flast = gc_internal_find_longname_flag(ans->lex.src + 2, length, map);
		ans->lex.src = eq + 1;
		err = gc_internal_parse_argument(ans);
	}

	if (ans->flast == NULL) {
		return GCE_UNKNOWN_LONGNAME;
	}
	ans->flast->seen = true;
	return err;
}

static enum GCError gc_internal_should_panic (const void *p, const char *msg) {
	if (p != NULL) {
		return GCE_NONE;
	}
#ifdef GC_ALLOW_GC_HANDLE_ERRORS
	fprintf(stderr, "\x1b[1mGC\x1b[0m: fatal error: %s\nAborting now!\n", msg);
	exit(EXIT_FAILURE);
#else
	return GCE_NO_HEAP_SPACE;
#endif
	return GCE_NONE;
}

static enum GCError gc_internal_parse_parg (struct GCAns *ans) {
	enum GCError err = GCE_NONE;

	if (ans->pargs.arguments == NULL) {
		ans->pargs.arguments = (char**) calloc(GC_INTERNAL_PARGS_STEP_FACTOR, sizeof(*ans->pargs.arguments));
		ans->pargs.__cap = GC_INTERNAL_PARGS_STEP_FACTOR;
		ans->pargs.total = 0;

		err = gc_internal_should_panic(ans->pargs.arguments, "failed at allocating memory for positional arguments array");
	}

	if (ans->pargs.total == ans->pargs.__cap) {
#ifdef GC_PARGS_FLEXIBLE
		ans->pargs.__cap += GC_INTERNAL_PARGS_STEP_FACTOR;
		ans->pargs.arguments = (char**) realloc(ans->pargs.arguments, ans->pargs.__cap * sizeof(*ans->pargs.arguments));
		err = gc_internal_should_panic(ans->pargs.arguments, "failed at expanding memory for positional arguments array");
#else
		return GCE_TOO_MANY_PARGS;
#endif
	}

	ans->pargs.arguments[ans->pargs.total++] = ans->lex.src;
	return err;
}

#ifdef GC_ALLOW_GC_HANDLE_ERRORS
static void gc_internal_handle_programmer_fault (const enum GCError err) {
	const char *const errors[] = {
		"invalid shortname",
		"duplicated shortname",
		"duplicated longname",
		"malformed flag's option field"
	};	
	fprintf(
		stderr,
		"\x1b[1mGC\x1b[0m: fatal error; aborting now: %s\n"
		"\tplease check documentation if the error persists!\n",
		errors[err - 1]
	);
	exit(EXIT_FAILURE);
}

static void gc_internal_handle_user_fault (const struct GCAns *const ans, const char *const pname) {
	switch (ans->err) {
		case GCE_UNKNOWN_SHORTNAME: {
			fprintf(stderr, "\x1b[1m%s\x1b[0m: error: unrecognizable `%c` short flag name\n", pname, ans->lex.src[ans->lex.pos]);
			break;
		}
		case GCE_MISSING_ARGUMENT: {
			fprintf(stderr, "\x1b[1m%s\x1b[0m: error: `%c` flag requires an argument, but none was provided\n", pname, ans->flast->shortname);
			break;
		}
		case GCE_NONSENSE_ARGUMENT: {
			fprintf(stderr, "\x1b[1m%s\x1b[0m: error: `%s` argument cannot be associated to any flag\n", pname, ans->lex.src);
			break;
		}
		case GCE_UNKNOWN_LONGNAME: {
			fprintf(stderr, "\x1b[1m%s\x1b[0m: error: `%s` cannot be recognized as a program's flag\n", pname, ans->lex.src);
			break;
		}
		case GCE_TOO_MANY_PARGS: {
			fprintf(stderr, "\x1b[1m%s\x1b[0m: error: %s, only accepts %d positional arguments, `%s` exceeds such limit\n", pname, pname, GC_INTERNAL_PARGS_STEP_FACTOR, ans->lex.src);
			break;
		}
		default: {
			fprintf(stderr, "\x1b[1mGC\x1b[0m: GC SHOULD HAVE NEVER REACHED THIS. PLEASE DO NOT MODIFY WITHOUT TESTING!\n");
			exit(EXIT_FAILURE);
		}
	}
	fprintf(stderr, "\tplease check usage if the error persists!\n");
	exit(EXIT_FAILURE);
}
#endif

GC_API struct GCAns gc_init (uint32_t argc, char **argv, struct GCFlag *flags, const uint32_t nflags, const char *pgmname) {
	struct GCAns ans;
	memset(&ans, 0, sizeof(ans));
	ans.err = gc_internal_check_integrity(flags, nflags);

	if (GC_IS_PROGRAMMER_FAULT(ans.err)) {
#ifdef GC_ALLOW_GC_HANDLE_ERRORS
		gc_internal_handle_programmer_fault(ans.err);
#else
		return ans;
#endif
	}
	if (argc == 0 || argv == NULL) {
		return ans;
	}

	struct GCMap map = { .nflags = nflags };
	gc_internal_map_flags(&map, flags);

	bool onlypargs = false;
	for (int i = 1; (i < argc) && (ans.err == GCE_NONE); i++) {
		ans.lex.src = argv[i];
		ans.lex.pos = 0;
		ans.lex.len = strlen(ans.lex.src);

		if (onlypargs) {
			ans.err = gc_internal_parse_parg(&ans);
			continue;
		}

		if (*ans.lex.src == '-' && ans.lex.len >= 2 && isalnum(ans.lex.src[1])) {
			ans.err = gc_internal_parse_shortopt(&map, &ans);
		}
		else if (ans.lex.len >= 3 && *ans.lex.src == '-' && ans.lex.src[1] == '-' && isalnum(ans.lex.src[2])) {
			ans.err = gc_internal_parse_longopt(&map, &ans);
		}
		else if (ans.lex.len == 2 && *ans.lex.src == '-' && ans.lex.src[1] == '-') {
			onlypargs = true;
			
		} else {
			ans.err = gc_internal_parse_argument(&ans);
		}
	}

	if (gc_internal_was_arg_set(ans.flast) == false) {
		ans.err = GCE_MISSING_ARGUMENT;
	}
#ifdef GC_ALLOW_GC_HANDLE_ERRORS
	if (GC_IS_USER_FAULT(ans.err)) {
		gc_internal_handle_user_fault(&ans, pgmname);
	}
	if (GC_IS_PROGRAMMER_FAULT(ans.err)) {
		gc_internal_handle_programmer_fault(ans.err);
	}
	if (GC_IS_SYS_FAULT(ans.err)) {
		gc_internal_should_panic(NULL, "internal error!");
	}
#endif
	return ans;
}

GC_API void gc_free (struct GCAns *ans) {
	if ((ans == NULL) || (ans->pargs.arguments == NULL)) {
		return;
	}
	ans->pargs.total = 0;
	ans->pargs.__cap = 0;
	free(ans->pargs.arguments);
}

#endif
#endif
