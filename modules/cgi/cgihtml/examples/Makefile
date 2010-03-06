# Makefile for cgihtml.a
# $Id: Makefile,v 1.2 2010/03/06 18:17:14 wes Exp $

# macros and variables
CC= gcc
CFLAGS= -g -Wall -DUNIX #-DUPLOADDIR=/tmp
LIB = ../cgihtml.a

INSTALLDIR= /usr/local/etc/httpd/cgi-bin
TARGETS = query-results mail.cgi index-sample.cgi ignore.cgi test.cgi

# targets
query-results: $(LIB) query-results.o
	$(CC) -o $@ query-results.o $(LIB)

mail.cgi: $(LIB) mail.cgi.o
	$(CC) -o $@ mail.cgi.o $(LIB)

index-sample.cgi: $(LIB) index-sample.cgi.o
	$(CC) -o $@ index-sample.cgi.o $(LIB)

ignore.cgi: $(LIB) ignore.cgi.o
	$(CC) -o $@ ignore.cgi.o $(LIB)

test.cgi: $(LIB) test.cgi.o
	$(CC) -o $@ test.cgi.o $(LIB)

$(LIB):
	cd ..; $(MAKE) cgihtml.a

all: $(TARGETS)

install: $(TARGETS)
	chmod a+x $(TARGETS)
	mv -f $(TARGETS) $(INSTALLDIR)

clean:
	rm -f *.o $(TARGETS)
