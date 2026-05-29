/* little demo on how GC works:
 *
 * -v                 no argument (boolean)
 * -v or --file       required argument (text)
 * -n or --number     required argument (u32)
 * -d or --debug      optional argument (i32)
 */
#define GC_IMPLEMENTATION
#include "gc.h"

static void found (struct GCFlag *flag) {
	printf("-------------------- flag found\n");
	printf("longname: %s\n", flag->longname);
	printf("shortname: %c\n", flag->shortname);
	if (!flag->aset) {
		printf("no argument\n");
		return;
	}
	switch (flag->opts & GC_INTERNAL_ARG_TYPE_MASK) {
		case GC_OPTS_FLAG_ARG_TYPE_TEXT: { printf("argument %s\n", flag->as.text); break; }
		case GC_OPTS_FLAG_ARG_TYPE_DOUB: { printf("argument %f\n", flag->as.doub); break; }
		case GC_OPTS_FLAG_ARG_TYPE_UI32: { printf("argument %u\n", flag->as.u32); break; }
		case GC_OPTS_FLAG_ARG_TYPE_I32 : { printf("argument %d\n", flag->as.i32); break; }
		case GC_OPTS_FLAG_ARG_TYPE_UI64: { printf("argument %lu\n", flag->as.u64); break; }
		case GC_OPTS_FLAG_ARG_TYPE_I64 : { printf("argument %ld\n", flag->as.i64); break; }
	}
}

int main (int argc, char **argv)
{
	struct GCFlag flags[] = {
		{
			.longname = NULL,
			.shortname = 'v',
			.opts = GC_OPTS_FLAG_NON_ARG
		},
		{
			.longname = "file",
			.shortname = 'f',
			.opts = GC_OPTS_FLAG_REQ_ARG | GC_OPTS_FLAG_ARG_TYPE_TEXT
		},
		{
			.longname = "number",
			.shortname = 'n',
			.opts = GC_OPTS_FLAG_REQ_ARG | GC_OPTS_FLAG_ARG_TYPE_UI32
		},
		{
			.longname = "debug",
			.shortname = 'd',
			.opts = GC_OPTS_FLAG_OPT_ARG | GC_OPTS_FLAG_ARG_TYPE_I32
		}
	};

	const uint32_t nflags = (sizeof(flags) / sizeof(*flags));
	struct GCAns ans = gc_init(argc, argv, flags, (uint32_t) (sizeof(flags) / sizeof(*flags)), "testing");

	for (uint32_t i = 0; i < nflags; i++) {
		if (flags[i].seen) {
			found(&flags[i]);
		}
	}


	printf("\npositional arguments:\n");
	printf("--------------------\n");
	for (uint32_t i = 0; i < ans.pargs.total; i++) {
		printf("PA%-3d: %s\n", i, ans.pargs.arguments[i]);
	}

	gc_free(&ans);
	return 0;
}
