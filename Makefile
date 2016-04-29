CC?=cc
CFLAGS?=-D_BSD_SOURCE
LDFLAGS?=-static

.PHONY: clean

conf: conf.o
	$(CC) $(LDFLAGS) -o conf conf.o  -lucl

clean:
	-$(RM) conf *.o
