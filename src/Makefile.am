MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = @DEFINES@

bin_PROGRAMS = event
noinst_LTLIBRARIES = libdb.la libcgi.la

event_SOURCES = event.cc event.hh event_main.cc common.hh cgi.hh db.hh
event_LDADD = libdb.la libcgi.la

libcgi_la_SOURCES = cgi.hh common.hh cgi.cc \
					query_parser.hh query_parser.cc \
					header_parser.hh header_parser.cc \
					strutils.hh strutils.cc \
					args.hh args.cc \
					http.hh http.cc \
				    multipart_formdata_parser.hh multipart_formdata_parser.cc
libcgi_la_CPPFLAGS = $(AM_CPPFLAGS) @FASTCGI_CFLAGS@
libcgi_la_LIBADD = @FASTCGI_LIBS@

libdb_la_SOURCES = db.hh common.hh db.cc sqlite3_db.hh sqlite3_db.cc
libdb_la_CPPFLAGS = $(AM_CPPFLAGS) @SQLITE3_CFLAGS@
libdb_la_LIBADD = @SQLITE3_LIBS@


