bios_bmc_smm_error_logger_pre = declare_dependency(
  dependencies: [
    dependency('threads'),
])

bios_bmc_smm_error_logger_dep = declare_dependency(
  dependencies: bios_bmc_smm_error_logger_pre,)

executable(
  'bios-bmc-smm-error-logger',
  'main.cpp',
  implicit_include_directories: false,
  dependencies: bios_bmc_smm_error_logger_dep,
  install: true,
)
