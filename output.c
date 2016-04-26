#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <err.h>
#include <sysexits.h>

#include <ucl.h>

#include "output.h"

static void oarr(ucl_object_t const *obj, bool pzero);
static void oakeys(ucl_object_t const *obj, bool pzero);
static void obool(ucl_object_t const *obj, bool pzero);
static void ofloat(ucl_object_t const *obj, bool pzero);
static void oint(ucl_object_t const *obj, bool pzero);
static void okeys(ucl_object_t const *obj, bool pzero);
static void ostring(ucl_object_t const *obj, bool pzero);

typedef void (*ofunc)(ucl_object_t const *, bool);

static struct {
	ofunc output;
 	ofunc keys;
	bool compound;
} odata[] = {
	{okeys, okeys, true}, /* UCL_OBJECT */ 
	{oarr, oakeys, true}, /* UCL_ARRAY */
	{oint, NULL, false}, /* UCL_INT */
	{ofloat, NULL, false}, /* UCL_FLOAT */
	{ostring, NULL, false}, /* UCL_STRING */
	{obool, NULL, false}, /*UCL_BOOLEAN */
	{ofloat, NULL, false}, /* UCL_TIME */
	{NULL, NULL, false}, /* UCL_USERDATA */
	{NULL, NULL, false}, /* UCL_NULL */
};

void
output(ucl_object_t const *obj, bool pzero)
{
	ofunc f;

	if ((f = odata[ucl_object_type(obj)].output) != NULL)
		f(obj, pzero);  
}

void
keys(ucl_object_t const *obj)
{
	ofunc f;

	if ((f = odata[ucl_object_type(obj)].keys) != NULL)
		f(obj, true);  
}

#define oprintf(p, f, ...) \
	if (!(p)) \
		return; \
	if (printf(f "\n", __VA_ARGS__) < 0) \
		err(EX_IOERR, "output error");

static void
okeys(ucl_object_t const *obj, bool pzero)
{
	ucl_object_iter_t it = NULL;
	ucl_object_t const *o;

	while ((o = ucl_object_iterate(obj, &it, true)) != NULL) {
		oprintf(true, "%s", ucl_object_key(o));
	}
}

static void
oakeys(ucl_object_t const *obj, bool pzero)
{
	int i = 0;
	ucl_object_iter_t it = NULL;
	ucl_object_t const *o;

	while ((o = ucl_object_iterate(obj, &it, true)) != NULL) {
		oprintf(true, "%d", i++);
	}
}


bool
allscalar(ucl_object_t const *obj)
{
	ucl_object_iter_t it = NULL;
	ucl_object_t const *o;

	while ((o = ucl_object_iterate(obj, &it, true)) != NULL) {
		if (odata[ucl_object_type(o)].compound)
			return false;
	}
	return true;
}

static void
oarr(ucl_object_t const *obj, bool pzero)
{
	ucl_object_iter_t it = NULL;
	ucl_object_t const *o;

	if (!allscalar(obj)) {
		oakeys(obj, false);
		return;
	}
	while ((o = ucl_object_iterate(obj, &it, true)) != NULL) {
		output(o, true);
	} 
}

static void
obool(ucl_object_t const *obj, bool pzero)
{
	bool b;

	b = ucl_object_toboolean(obj);
	oprintf(pzero || b, "%s", b ? "true" : "false");
}

static void
ofloat(ucl_object_t const *obj, bool pzero)
{
	double f;

	f = ucl_object_todouble(obj);
	oprintf(pzero || f != 0.0, "%0.10f", f);
}

static void
oint(ucl_object_t const *obj, bool pzero)
{
	int64_t i;

	i = ucl_object_toint(obj);
	oprintf(pzero || i != 0, "%" PRId64, i);
}

static void
ostring(ucl_object_t const *obj, bool pzero)
{
	const char * s;

	s = ucl_object_tostring(obj);
	oprintf(pzero || *s != '\0', "%s", s);
}
