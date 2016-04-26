CFLAGS?=-D_BSD_SOURCE --std=c99
LDFLAGS?=-static

.PHONY: clean indent

conf: buf.o main.o output.o
	$(CC) $(LDFLAGS) -o conf main.o buf.o output.o -lucl -lbsd

clean:
	-$(RM) conf buf.o main.o output.o
