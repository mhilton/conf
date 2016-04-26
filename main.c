#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <err.h>
#include <sysexits.h>

#include <ucl.h>

#include "buf.h"
#include "output.h"

static bool ozero = true;
static bool okeys = false;
static char *file = NULL;
static char *key  = NULL;

/*
 * usage prints usage information to stderr and exits.
 */
static void
usage(char *pname)
{

	fprintf(stderr, "usage: %s [-kz] [-f file] [key]\n", pname);
	exit(EX_USAGE);
}

/*
 * parseopt parses the supplied options.
 */
static void
parseopt(int argc, char **argv)
{
	int c;

	while ((c = getopt(argc, argv, "f:kz")) != -1) {
		switch (c) {
		case 'f':
			file = optarg;
			break;
		case 'k':
			okeys = true;
			break;
		case 'z':
			ozero = false;
			break;
		case ':':
		case '?':
			usage(argv[0]);
		}
	}
	if (optind < argc) 
		key = argv[optind++];
}

/*
 * keypart returns the next part of the specified key, or NULL 
 * if there are no more parts.
 */
static const char *
keypart()
{
	const char *start;

	if (key == NULL || *key == '\0')
		return NULL;
	while (*key == '.')
		key++;
	if (key[0] == '\0')
		return NULL;
	start = key;
	while (*key != '.' && *key != '\0')
		key++;
	if (*key == '.')
		*key++ = '\0';
	return start;
}

/*
 * findkey finds the object matching the supplied key, or NULL if there is
 * no such object.
 */
static ucl_object_t const *
findkey(ucl_object_t const *obj)
{
	const char *k;
	long       n;

	while ((k = keypart()) != NULL && obj != NULL) {
		switch (ucl_object_type(obj)) {
		case UCL_OBJECT:
			obj = ucl_object_lookup(obj, k);
			break;
		case UCL_ARRAY: {
			char *end;

			n = strtol(k, &end, 10);
			if (*end == '\0')
				obj = ucl_array_find_index(obj, n);
			else 
				obj = NULL;
			break;
		}
		default:
			obj = NULL;
			break;	
		}
	}
	return obj;
}

/*
 * addvars adds a variable to the parser for each variable currently defined
 * in the environment.
 */
static void
addvars(struct ucl_parser *parser, char **envp)
{
	buf_t buf;
	char  *val;

	buf = BUF_ZERO;
	for (; *envp != NULL; envp++) {
		buf = buf_putz(buf, *envp);
		if (buf_error(buf))
			err(EX_OSERR, "cannot add variables");
		val = strchr(buf.p, '=');
		*val++ = '\0';
		ucl_parser_register_variable(parser, buf.p, val);
		buf = buf_trunc(buf, 0);
	}
	buf_free(buf);
}

int
main(int argc, char **argv, char **envp)
{
	char const         *error;
	ucl_object_t       *obj;
	const char         *key;
	ucl_object_t const *kobj;
	struct ucl_parser  *parser;

	parseopt(argc, argv);
	if (optind < argc) {
		usage(argv[0]);
	} 
	parser = ucl_parser_new(0);
	if (parser == NULL) {
		err(EX_OSERR, "cannot create parser");
	}
	addvars(parser, envp);

	if (file == NULL) {
		buf_t buf;

		buf = buf_readfile(BUF_ZERO, stdin);
		if (buf_error(buf))
			err(EX_IOERR, "cannot read stdin");
		ucl_parser_add_chunk(parser, buf.p, buf.len);
		buf_free(buf);
	} else {
		ucl_parser_add_file(parser, file);
	}

	if ((error = ucl_parser_get_error(parser))) {
		errx(EX_DATAERR, "cannot load %s: %s", file, error);
	}

	obj = ucl_parser_get_object(parser);
	kobj = findkey(obj);
	if (kobj != NULL)
		if (okeys)
			keys(kobj);
		else
			output(kobj, ozero);

	ucl_object_unref(obj);
	ucl_parser_free(parser);
}
