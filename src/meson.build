bios_bmc_smm_error_logger_pre = declare_dependency(
    include_directories: [root_inc, bios_bmc_smm_error_logger_inc],
    dependencies: [dependency('threads'), dependency('stdplus')],
)

bios_bmc_smm_error_logger_lib = static_library(
    'bios_bmc_smm_error_logger',
    'pci_handler.cpp',
    'buffer.cpp',
    implicit_include_directories: false,
    dependencies: bios_bmc_smm_error_logger_pre,
)

bios_bmc_smm_error_logger_dep = declare_dependency(
    link_with: bios_bmc_smm_error_logger_lib,
    dependencies: bios_bmc_smm_error_logger_pre,
)

libfuzzer_args = [
  '-fsanitize=fuzzer,address',  # Add 'undefined' or 'memory' if needed
  '-fno-omit-frame-pointer',
  '-g'
]

libfuzzer_dep = declare_dependency(
  compile_args: libfuzzer_args,
  link_args: libfuzzer_args
)

exe_sources = ['read_loop.cpp']
exe_dependencies = [bios_bmc_smm_error_logger_dep, rde_dep]

if get_option('fuzzing')
  exe_sources += ['fuzzer.cpp']
  exe_dependencies += [libfuzzer_dep]
else
  exe_sources += ['main.cpp']
endif

executable(
    'bios-bmc-smm-error-logger',
    exe_sources,
    conf_h,
    implicit_include_directories: false,
    dependencies: exe_dependencies,
    install: true,
    install_dir: get_option('bindir'),
)
