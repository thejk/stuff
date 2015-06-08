MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = @DEFINES@ -DLOCALSTATEDIR='"@localstatedir@/stuff"' \
			  -DSYSCONFDIR='"@sysconfdir@/stuff"'

bin_PROGRAMS = event sender
noinst_LTLIBRARIES = libdb.la libcgi.la libutil.la

event_SOURCES = event.cc event.hh event_main.cc common.hh cgi.hh db.hh \
				sender_client.cc sender_client.hh
event_LDADD = libdb.la libcgi.la

sender_SOURCES = common.hh sender.cc json.hh json.cc
sender_CPPFLAGS = $(AM_CPPFLAGS) @CURL_CFLAGS@
sender_LDADD = libutil.la @CURL_LIBS@

libcgi_la_SOURCES = cgi.hh common.hh cgi.cc \
					query_parser.hh query_parser.cc \
					header_parser.hh header_parser.cc \
					args.hh args.cc \
					http.hh http.cc \
				    multipart_formdata_parser.hh multipart_formdata_parser.cc
libcgi_la_CPPFLAGS = $(AM_CPPFLAGS) @FASTCGI_CFLAGS@
libcgi_la_LIBADD = @FASTCGI_LIBS@ libutil.la

libdb_la_SOURCES = db.hh common.hh db.cc sqlite3_db.hh sqlite3_db.cc
libdb_la_CPPFLAGS = $(AM_CPPFLAGS) @SQLITE3_CFLAGS@
libdb_la_LIBADD = @SQLITE3_LIBS@

libutil_la_SOURCES = common.hh fsutils.cc fsutils.hh config.cc config.hh \
					 strutils.hh strutils.cc sockutils.hh sockutils.cc

