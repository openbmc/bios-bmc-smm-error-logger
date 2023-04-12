project(
  'bios-bmc-smm-error-logger',
  'cpp',
  version: '0.1',
  meson_version: '>=0.58.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',
    'werror=true',
  ])

root_inc = include_directories('.')
bios_bmc_smm_error_logger_inc = include_directories('include')
rde_inc = include_directories('include')

# Setting up config data
conf_data = configuration_data()

conf_data.set('READ_INTERVAL_MS', get_option('read-interval-ms'))

conf_data.set('MEMORY_REGION_SIZE', get_option('memory-region-size'))
conf_data.set('MEMORY_REGION_OFFSET', get_option('memory-region-offset'))
conf_data.set('BMC_INTERFACE_VERSION', get_option('bmc-interface-version'))

conf_data.set('QUEUE_REGION_SIZE', get_option('queue-region-size'))
conf_data.set('UE_REGION_SIZE', get_option('ue-region-size'))
conf_data.set('MAGIC_NUMBER_BYTE1', get_option('magic-number-byte1'))
conf_data.set('MAGIC_NUMBER_BYTE2', get_option('magic-number-byte2'))
conf_data.set('MAGIC_NUMBER_BYTE3', get_option('magic-number-byte3'))
conf_data.set('MAGIC_NUMBER_BYTE4', get_option('magic-number-byte4'))

conf_h = configure_file(
  output: 'config.h',
  configuration: conf_data)

subdir('src/rde')
subdir('src')
if not get_option('tests').disabled()
  subdir('test')
endif

# installation of systemd service files
subdir('service_files')
