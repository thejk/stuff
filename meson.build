project(
  'stuff', 'cpp',
  version : '0.2',
  meson_version: '>= 0.59',
  default_options : [
    'warning_level=3',
    'cpp_std=c++17',
    'cpp_rtti=false',
    'cpp_eh=none',
  ],
)

cpp = meson.get_compiler('cpp')
cpp_flags = cpp.get_supported_arguments(
  '-Wno-unused-parameter',
  checked: 'off',
)
if get_option('buildtype') == 'release'
  cpp_flags += '-DNDEBUG'
endif

add_global_arguments(cpp_flags, language: 'cpp')

add_project_arguments(['-DHAVE_CONFIG_H'], language: 'cpp')

conf_data = configuration_data()

# 3.6.5 so that sqlite3_changes() return correct values for DELETE
sqlite3_dep = dependency('sqlite3', version: '>= 3.6.5')

curl_dep = dependency('libcurl', version: '>= 7.25.0')

# It is really weird that fcgi++ doesn't depend on fcgi
fastcgipp_dep = dependency('fcgi++', version: '>= 2.4.2', required: false)
fastcgi_dep = dependency('fcgi', version: '>= 2.4.2', required: false)

conf_data.set10('HAVE_FASTCGI', fastcgipp_dep.found() and fastcgi_dep.found())
conf_data.set_quoted('SYSCONFDIR', join_paths(get_option('prefix'),
                                              get_option('sysconfdir')))
conf_data.set_quoted('LOCALSTATEDIR', join_paths(get_option('prefix'),
                                                 get_option('localstatedir')))
configure_file(output: 'config.h',
               configuration: conf_data)

inc = include_directories('src')

util_lib = static_library(
  'util',
  'src/fsutils.cc',
  'src/config.cc',
  'src/strutils.cc',
  'src/sockutils.cc',
  'src/base64.cc',
  gnu_symbol_visibility: 'hidden',
  include_directories: inc,
  install: false,
)
util_dep = declare_dependency(
  include_directories: inc,
  link_with: util_lib,
)

json_lib = static_library(
  'json',
  'src/json.cc',
  gnu_symbol_visibility: 'hidden',
  include_directories: inc,
  install: false,
)
json_dep = declare_dependency(
  include_directories: inc,
  link_with: json_lib,
)

cgi_deps = [
  fastcgi_dep,
  fastcgipp_dep,
  util_dep,
]
cgi_lib = static_library(
  'cgi',
  'src/cgi.cc',
  'src/query_parser.cc',
  'src/header_parser.cc',
  'src/args.cc',
  'src/http.cc',
  'src/auth.cc',
  'src/multipart_formdata_parser.cc',
  dependencies: cgi_deps,
  gnu_symbol_visibility: 'hidden',
  include_directories: inc,
  install: false,
)
cgi_dep = declare_dependency(
  dependencies: cgi_deps,
  include_directories: inc,
  link_with: cgi_lib,
)

db_deps = [
  sqlite3_dep,
]
db_lib = static_library(
  'db',
  'src/db.cc',
  'src/sqlite3_db.cc',
  dependencies: db_deps,
  gnu_symbol_visibility: 'hidden',
  include_directories: inc,
  install: false,
)
db_dep = declare_dependency(
  dependencies: db_deps,
  include_directories: inc,
  link_with: db_lib,
)

sender_client_deps = [
  util_dep,
]
sender_client_lib = static_library(
  'sender_client',
  'src/sender_client.cc',
  dependencies: sender_client_deps,
  gnu_symbol_visibility: 'hidden',
  include_directories: inc,
  install: false,
)
sender_client_dep = declare_dependency(
  dependencies: sender_client_deps,
  include_directories: inc,
  link_with: sender_client_lib,
)

event_deps = [
  db_dep,
  sender_client_dep,
]
event_lib = static_library(
  'event',
  'src/event.cc',
  'src/event_utils.cc',
  dependencies: event_deps,
  gnu_symbol_visibility: 'hidden',
  include_directories: inc,
  install: false,
)
event_dep = declare_dependency(
  dependencies: event_deps,
  include_directories: inc,
  link_with: event_lib,
)

executable(
  'event',
  'src/event_main.cc',
  dependencies: [
    cgi_dep,
    event_dep,
  ],
  install: true,
)

executable(
  'page',
  'src/page_main.cc',
  dependencies: [
    cgi_dep,
    event_dep,
  ],
  install: true,
)

executable(
  'sender',
  'src/sender.cc',
  dependencies: [
    curl_dep,
    json_dep,
    util_dep,
  ],
  install: true,
)

executable(
  'test-sender',
  'src/test_sender.cc',
  dependencies: [
    sender_client_dep,
  ],
)

test(
  'query-parser',
  executable(
    'test-query-parser',
    'test/test-query-parser.cc',
    dependencies: [
      cgi_dep,
    ],
  )
)

test(
  'header-parser',
  executable(
    'test-header-parser',
    'test/test-header-parser.cc',
    dependencies: [
      cgi_dep,
    ],
  )
)

test(
  'multipart-formatdata-parser',
  executable(
    'test-multipart-formatdata-parser',
    'test/test-multipart-formdata-parser.cc',
    dependencies: [
      cgi_dep,
    ],
  )
)

test(
  'args',
  executable(
    'test-args',
    'test/test-args.cc',
    dependencies: [
      cgi_dep,
    ],
  )
)

test(
  'json',
  executable(
    'test-json',
    'test/test-json.cc',
    dependencies: [
      json_dep,
    ],
  )
)
