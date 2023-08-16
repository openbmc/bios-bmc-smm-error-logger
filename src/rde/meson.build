
cpp = meson.get_compiler('cpp')
if cpp.has_header('nlohmann/json.hpp')
    nlohmann_json_dep = declare_dependency()
else
    subproject('nlohmann', required: false)
    nlohmann_json_dep = declare_dependency(
        include_directories: [
            'subprojects/nlohmann/single_include',
            'subprojects/nlohmann/single_include/nlohmann',
        ]
    )
endif

rde_pre = declare_dependency(
  include_directories: [rde_inc],
  dependencies: [
    nlohmann_json_dep,
    dependency('libbej'),
    dependency('phosphor-dbus-interfaces'),
    dependency('sdbusplus'),
  ]
)

rde_lib = static_library(
  'rde',
  'rde_dictionary_manager.cpp',
  'external_storer_file.cpp',
  'rde_handler.cpp',
  'notifier_dbus_handler.cpp',
  implicit_include_directories: false,
  dependencies: rde_pre)

rde_dep = declare_dependency(
  link_with: rde_lib,
  dependencies: rde_pre)
