project(
  'bios-bmc-smm-error-logger',
  'cpp',
  version: '0.1',
  meson_version: '>=0.57.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',
    'werror=true',
  ])

root_inc = include_directories('.')
bios_bmc_smm_error_logger_inc = include_directories('include')
rde_inc = include_directories('include')

subdir('src')
subdir('src/rde')
subdir('test')

# installation of systemd service files
subdir('service_files')
