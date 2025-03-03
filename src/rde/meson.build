rde_pre = declare_dependency(
    include_directories: [rde_inc],
    dependencies: [
        dependency('libbej'),
        dependency('nlohmann_json', include_type: 'system'),
        dependency('phosphor-dbus-interfaces'),
        dependency('sdbusplus'),
        dependency('stdplus'),
    ],
)

rde_lib = static_library(
    'rde',
    'rde_dictionary_manager.cpp',
    'external_storer_file.cpp',
    'rde_handler.cpp',
    'notifier_dbus_handler.cpp',
    implicit_include_directories: false,
    dependencies: rde_pre,
)

rde_dep = declare_dependency(link_with: rde_lib, dependencies: rde_pre)
