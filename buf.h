/*
 * buf_t represents a buffer that tracks it's length and capacity. Clients
 * may access p, len or cap; but must treat them as read only values.
 */
typedef struct {
	uint8_t *p;
	size_t   len;
	size_t   cap;
} buf_t;

/*
 * BUF_ZERO is a zero length buffer that may be used as the input to any
 * buf_* function.
 */
#define BUF_ZERO ((buf_t) {NULL, 0, 0})

/*
 * buf_error returns true if the supplied buf_t indicates there was an
 * error in the previous buf_* operation.
 */
bool  buf_error(buf_t);

/*
 * buf_free frees the resources associated with the supplied buf_t.
 */
void  buf_free(buf_t);

/*
 * buf_readfile appends the contents of the supplied file onto the supplied
 * buf_t. The returned buf_t will either have the whole file appended or
 * will cause buf_error to return true.
 */
buf_t buf_readfile(buf_t, FILE *);

/*
 * buf_puts appends the supplied string to the supplied buf_t. The returned
 * buf_t will either have the whole string appended or will cause buf_error
 * to return true.
 */
buf_t buf_puts(buf_t, const char *);

/* 
 * buf_putz is like buf_puts, except that it also includes the '\0' at the
 * end of the string.
 */
buf_t buf_putz(buf_t, const char *);

/* 
 * buf_trunc truncates the supplied buf_t to have a length of at most the
 * specified length.
 */
buf_t buf_trunc(buf_t, size_t);