MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = @DEFINES@

noinst_LTLIBRARIES = libdb.la

libdb_la_SOURCES = db.hh common.hh db.cc sqlite3_db.hh sqlite3_db.cc
libdb_la_CPPFLAGS = $(AM_CPPFLAGS) @SQLITE3_CFLAGS@
libdb_la_LIBADD = @SQLITE3_LIBS@


