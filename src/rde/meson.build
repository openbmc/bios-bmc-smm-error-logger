libbej_dep = dependency('libbej', required : false)
if not libbej_dep.found()
  libbej_proj = subproject('libbej')
  libbej_dep = libbej_proj.get_variable('libbej')
endif

rde_pre = declare_dependency(
  include_directories: [rde_inc],
  dependencies: [libbej_dep]
  )

rde_lib = static_library(
  'rde',
  'rde_dictionary_manager.cpp',
  'external_storer_file.cpp',
  'rde_handler.cpp',
  implicit_include_directories: false,
  dependencies: rde_pre)

rde_dep = declare_dependency(
  link_with: rde_lib,
  dependencies: rde_pre)
