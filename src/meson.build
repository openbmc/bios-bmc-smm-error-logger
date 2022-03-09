fmt_dep = dependency('fmt', required: false)
if not fmt_dep.found()
  fmt_opts = import('cmake').subproject_options()
  fmt_opts.add_cmake_defines({
    'CMAKE_POSITION_INDEPENDENT_CODE': 'ON',
    'MASTER_PROJECT': 'OFF',
  })
  fmt_proj = import('cmake').subproject(
    'fmt',
    options: fmt_opts,
    required: false)
  assert(fmt_proj.found(), 'fmtlib is required')
  fmt_dep = fmt_proj.dependency('fmt')
endif

bios_bmc_smm_error_logger_pre = declare_dependency(
  include_directories: [root_inc, bios_bmc_smm_error_logger_inc],
  dependencies: [
    fmt_dep,
    dependency('threads'),
    dependency('stdplus'),
  ])

bios_bmc_smm_error_logger_lib = static_library(
  'bios_bmc_smm_error_logger',
  'pci_handler.cpp',
  implicit_include_directories: false,
  dependencies: bios_bmc_smm_error_logger_pre)

bios_bmc_smm_error_logger_dep = declare_dependency(
  link_with: bios_bmc_smm_error_logger_lib,
  dependencies: bios_bmc_smm_error_logger_pre)

executable(
  'bios-bmc-smm-error-logger',
  'main.cpp',
  implicit_include_directories: false,
  dependencies: bios_bmc_smm_error_logger_dep,
  install: true,
  install_dir: get_option('bindir')
)
