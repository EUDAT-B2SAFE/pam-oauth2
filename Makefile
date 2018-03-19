CC=gcc

CFLAGS=-Wall -fPIC -ansi -pedantic
DEST=/lib/security

all: pam_oauth2.so

jsmn/libjsmn.a:
	$(MAKE) CFLAGS="$(CFLAGS)" -C jsmn

parson/libparson.a:
	$(MAKE) CFLAGS="$(CFLAGS)" -C parson

pam_oauth2.so: pam_oauth2.o jsmn/libjsmn.a parson/libparson.a
	$(CC) -shared $^ -lcurl -lconfig -o $@

install: pam_oauth2.so
	if ! test -d $(DEST); then mkdir $(DEST); fi
	cp -a pam_oauth2.so $(DEST)

clean:
	rm -f *.o *.so
