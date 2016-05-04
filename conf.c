#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <err.h>
#include <sysexits.h>

#include <ucl.h>

#include "config.h"

static char envkey[MAX_KEY_LEN];
static char envval[MAX_VALUE_LEN];
static void *data = NULL;

static int cmdarg = 0;
static bool env = false;
static char *file = NULL;
static char *key = NULL;
static char *name = NULL;

/*
 * usage prints usage information to stderr and exits.
 */
static void
usage()
{

	fprintf(stderr, 
"usage: conf [-f file] [key]\n"
"            [-e [-f file] [-n name] key cmd...]\n");
	exit(EX_USAGE);
}

/*
 * parseopt parses the supplied options.
 */
static void
parseopt(int argc, char **argv)
{
	int c;

	while ((c = getopt(argc, argv, "ef:n:")) != -1) {
		switch (c) {
		case 'e':
			env = true;
			break;
		case 'f':
			file = optarg;
			break;
		case 'n':
			name = optarg;
			break;
		case ':':
		case '?':
			usage();
		}
	}
	if (optind < argc) 
		key = argv[optind++];
	if (optind < argc) {
		if (!env)
			usage();
		cmdarg = optind++;
	}
	if (optind < argc) {
		usage();
	}
}

/*
 * strlcpychr copies up to l - 1 bytes from src to dest, stopping at the
 * first occurence of either c or '\0'. The return value is the locaiton of
 * the first occurence of c in src, or the length of src if c is not present.
 */
static size_t
strlcpychr(char *dst, const char *src, size_t size, int c)
{
	size_t len;

	for (len = 0; len < size -1 && *src != '\0' && *src != c; len++)
		*dst++ = *src++;
	if (size > 0)
		*dst = '\0';
	for (; *src != '\0' && *src != c; src++)
		len++;
	return len;
}

/*
 * addvars adds a variable to the parser for each variable currently defined
 * in the environment.
 */
static void
addvars(struct ucl_parser *parser, char **envp)
{
	size_t len;

	for (; *envp != NULL; envp++) {
		len = strlcpychr(envkey, *envp, MAX_KEY_LEN, '=');
		if (len >= MAX_KEY_LEN)
			continue;
		ucl_parser_register_variable(parser, envkey, *envp + len + 1);
	}
}

/*
 * parse_file loads the file supplied on the command line, or stdin, and
 * parses it using parser.
 */
static void
parse_file(struct ucl_parser  *parser)
{
	int fd = 0;
	struct stat s;

	if (file != NULL) {
		if ((fd = open(file, O_RDONLY)) == -1)
			err(EX_NOINPUT, "cannot open \"%s\"", file);
	} else {
		file = STDIN_NAME;
	}
	if (fstat(fd, &s) == -1)
		err(EX_NOINPUT, "cannot read %s", file);
	if (S_ISREG(s.st_mode))
		data = mmap(NULL, s.st_size, PROT_READ, MAP_SHARED, fd, 0);
	else
		data = mmap(NULL, MAX_STDIN_LEN, PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (data == MAP_FAILED)
		err(EX_NOINPUT, "cannot read %s", file);
	if (!S_ISREG(s.st_mode)) {
		int n;

		while (s.st_size < MAX_STDIN_LEN) {
			n = read(fd, data + s.st_size, MAX_STDIN_LEN - s.st_size);
			if (n == -1)
				err(EX_NOINPUT, "cannot read %s", file);
			if (n == 0)
				break;
			s.st_size += n;
		}
		if (s.st_size == MAX_STDIN_LEN && n != 0)
			err(EX_DATAERR, "%s too long", file);
	}	
	ucl_parser_add_chunk_full(parser, data, s.st_size, 0, UCL_DUPLICATE_MERGE, UCL_PARSE_UCL);
}

/*
 * snprintkeys writes the keys contained in obj to dst.
 */
static int
snprintkeys(char *dst, size_t size, ucl_object_t const *obj)
{
	ucl_object_iter_t it = NULL;
	ucl_object_t const *o;
	int n = 0;
	int i = 0;

	switch (ucl_object_type(obj)) {
	case UCL_OBJECT:
		for (i = 0; (o = ucl_iterate_object(obj, &it, true)) != NULL; i++) {
			int l;

			l = snprintf(dst + n, (size - n > 0) ? size - n: 0, "%s%s", (i == 0)?"":"\n", ucl_object_key(o));
			if (l < 0)
				return l;
			n += l;
		}
		break;
	case UCL_ARRAY: {
		for (i = 0; (o = ucl_iterate_object(obj, &it, true)) != NULL; i++) {
			int l;

			l = snprintf(dst + n, (size - n > 0) ? size - n: 0, "%s%d", (i == 0)?"":"\n", i);
			if (l < 0)
				return l;
			n += l;
		}
		break;
	}
	default:
		*dst = '\0';
		break;
	}
	return n;
}

/*
 * snprintkeys writes the value contained in obj to dst.
 */
static int
snprintval(char *dst, size_t size, ucl_object_t const *obj)
{

	switch (ucl_object_type(obj)) {
	case UCL_INT:
		return snprintf(dst, size, "%" PRId64, ucl_object_toint(obj));
	case UCL_FLOAT:
	case UCL_TIME:
		return snprintf(dst, size, "%0.10f", ucl_object_todouble(obj));
	case UCL_STRING:
		return snprintf(dst, size, "%s", ucl_object_tostring(obj));
	case UCL_BOOLEAN:
		return snprintf(dst, size, "%s", ucl_object_toboolean(obj) ? "true": "false");
	default:
		*dst = '\0';
		return 0;
	}
}

/*
 * print outputs the appropriate data for obj. If obj is a UCL_OBJECT or
 * UCL_ARRAY a list if keys is printed, otherwise the value is printed.
 */
static void
print(ucl_object_t const *obj)
{
	int n;

	switch (ucl_object_type(obj)) {
	case UCL_OBJECT:
	case UCL_ARRAY:
		n = snprintkeys(envval, MAX_VALUE_LEN, obj);
		if (n < 0)
			errx(EX_SOFTWARE, "error listing keys");
		break;
	default:
		n = snprintval(envval, MAX_VALUE_LEN, obj);
		if (n < 0)
			errx(EX_SOFTWARE, "error formatting value");
		break;
	}
	if (n >= MAX_VALUE_LEN)
		errx(EX_DATAERR, "output too long");
	if (n > 0 && puts(envval) < 0)
		err(EX_IOERR, "cannot write value");
}

/*
 * expand extrapolates one or more environment variables from obj. If obj
 * is of type UCL_OBJECT or UCL_ARRAY then a new variable will be created
 * will be created containing a list of keys in obj and then expand will be
 * called recursively for each member. Otherwise a new environment variable
 * will be created with the value of obj.
 */
static void
expand(size_t keylen, ucl_object_t const *obj)
{
	ucl_object_iter_t it = NULL;
	ucl_object_t const *o;
	int i, l;

	switch (ucl_object_type(obj)) {
	case UCL_OBJECT:
		if (snprintkeys(envval, MAX_VALUE_LEN, obj) >= MAX_VALUE_LEN)
			errx(EX_DATAERR, "value too long \"%s\"", envval);
		setenv(envkey, envval, 1);
		while ((o = ucl_iterate_object(obj, &it, true)) != NULL) {
			l = snprintf(envkey + keylen, MAX_KEY_LEN - keylen, "_%s", ucl_object_key(o));
			if (l >= MAX_KEY_LEN - keylen)
				errx(EX_DATAERR, "key too long \"%s\"", envkey);
			expand(keylen + l, o);
		}
		break;
	case UCL_ARRAY:
		if (snprintkeys(envval, MAX_VALUE_LEN, obj) >= MAX_VALUE_LEN)
			errx(EX_DATAERR, "value too long \"%s\"", envval);
		setenv(envkey, envval, 1);
		i = 0;
		while ((o = ucl_iterate_object(obj, &it, true)) != NULL) {
			l = snprintf(envkey + keylen, MAX_KEY_LEN - keylen, "_%d", i++);
			if (l > MAX_KEY_LEN - keylen)
				errx(EX_DATAERR, "key too long \"%s\"", envkey);
			expand(keylen + l, o);
		}
		break;
	default:
		if (snprintval(envval, MAX_VALUE_LEN, obj) >= MAX_VALUE_LEN)
			errx(EX_DATAERR, "value too long \"%s\"", envval);
		setenv(envkey, envval, 1);
		break;
	}
}

/*
 * run executes the supplied command, and waits for it to complete.
 */
static int
run(char **argv)
{
	int status;

	switch (fork()) {
	case -1:
		err(EX_OSERR, "error starting command");
		break;
	case 0:
		execvp(argv[0], argv);
		// unreachable
	default:
		wait(&status);
	}
	return status;	
}

int
main(int argc, char **argv, char **envp)
{
	char const *error;
	ucl_object_t *obj;
	ucl_object_t const *kobj;
	struct ucl_parser *parser;
	int status = 0;

	parseopt(argc, argv);

	parser = ucl_parser_new(UCL_PARSER_NO_IMPLICIT_ARRAYS | UCL_PARSER_ZEROCOPY);
	if (parser == NULL) {
		err(EX_OSERR, "cannot create parser");
	}
	addvars(parser, envp);
	parse_file(parser);

	if ((error = ucl_parser_get_error(parser))) {
		errx(EX_DATAERR, "cannot load %s: %s", file, error);
	}

	obj = ucl_parser_get_object(parser);
	if (key != NULL && strcmp(key, ".") != 0)
		kobj = ucl_object_lookup_path(obj, key);
	else
		kobj = obj;
	if (env) {
		int nlen = 0;
		char *s;

		if (name != NULL)
			nlen = strlcpy(envkey, name, MAX_KEY_LEN);
		else if ((s = strrchr(key, '.')) != NULL)
			nlen = strlcpy(envkey, s + 1, MAX_KEY_LEN);
		if (kobj != NULL)
			expand(nlen, kobj);
		status = run(argv + cmdarg);
	} else if (kobj != NULL) {
		print(kobj);
	}

	ucl_object_unref(obj);
	ucl_parser_free(parser);
	exit(status);
}
