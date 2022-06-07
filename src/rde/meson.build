rde_lib = static_library(
  'rde',
  'rde_dictionary_manager.cpp',
  'external_storer_file.cpp',
  include_directories : rde_inc,
  implicit_include_directories: false)

rde_dep = declare_dependency(
  link_with: rde_lib)
