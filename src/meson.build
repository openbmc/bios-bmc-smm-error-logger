bios_bmc_smm_error_logger_pre = declare_dependency(
  include_directories: [root_inc, bios_bmc_smm_error_logger_inc],
  dependencies: [
    dependency('threads'),
    sys_dep,
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
