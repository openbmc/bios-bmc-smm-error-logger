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

subdir('src')

# installation of systemd service files
subdir('service_files')
