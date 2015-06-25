MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = @DEFINES@ -DLOCALSTATEDIR='"@localstatedir@/stuff"' \
			  -DSYSCONFDIR='"@sysconfdir@/stuff"'

bin_PROGRAMS = event sender page
noinst_PROGRAMS = test-sender
noinst_LTLIBRARIES = libdb.la libcgi.la libutil.la libsender_client.la \
					 libjson.la libevent.la

event_SOURCES = event_main.cc common.hh cgi.hh
event_LDADD = libcgi.la libevent.la

page_SOURCES = page_main.cc common.hh cgi.hh
page_LDADD = libcgi.la libevent.la

sender_SOURCES = common.hh sender.cc
sender_CPPFLAGS = $(AM_CPPFLAGS) @CURL_CFLAGS@
sender_LDADD = libjson.la libutil.la @CURL_LIBS@

test_sender_SOURCES = common.hh test_sender.cc sender_client.hh
test_sender_LDADD = libsender_client.la

libevent_la_SOURCES = event.cc event.hh event_utils.cc event_utils.hh \
					  common.hh db.hh sender_client.hh
libevent_la_LIBADD = libdb.la libsender_client.la

libjson_la_SOURCES = json.hh json.cc common.hh

libcgi_la_SOURCES = cgi.hh common.hh cgi.cc \
					query_parser.hh query_parser.cc \
					header_parser.hh header_parser.cc \
					args.hh args.cc \
					http.hh http.cc \
					auth.hh auth.cc \
					multipart_formdata_parser.hh multipart_formdata_parser.cc
libcgi_la_CPPFLAGS = $(AM_CPPFLAGS) @FASTCGI_CFLAGS@
libcgi_la_LIBADD = @FASTCGI_LIBS@ libutil.la

libdb_la_SOURCES = db.hh common.hh db.cc sqlite3_db.hh sqlite3_db.cc
libdb_la_CPPFLAGS = $(AM_CPPFLAGS) @SQLITE3_CFLAGS@
libdb_la_LIBADD = @SQLITE3_LIBS@

libutil_la_SOURCES = common.hh fsutils.cc fsutils.hh config.cc config.hh \
					 strutils.hh strutils.cc sockutils.hh sockutils.cc \
					 base64.hh base64.cc

libsender_client_la_SOURCES = common.h sender_client.cc sender_client.hh
libsender_client_la_LIBADD = libutil.la
