MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = @DEFINES@ -I$(top_builddir)/src

TESTS = test-query-parser test-header-parser test-multipart-formdata-parser \
		test-args test-json

check_PROGRAMS = $(TESTS)

test_query_parser_SOURCES = test-query-parser.cc
test_query_parser_LDADD = $(top_srcdir)/src/libcgi.la

test_header_parser_SOURCES = test-header-parser.cc
test_header_parser_LDADD = $(top_srcdir)/src/libcgi.la

test_multipart_formdata_parser_SOURCES = test-multipart-formdata-parser.cc
test_multipart_formdata_parser_LDADD = $(top_srcdir)/src/libcgi.la

test_args_SOURCES = test-args.cc
test_args_LDADD = $(top_srcdir)/src/libcgi.la

test_json_SOURCES = test-json.cc
test_json_LDADD = $(top_srcdir)/src/libjson.la
