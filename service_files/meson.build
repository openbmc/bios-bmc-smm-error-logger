systemd = dependency('systemd')
conf_data = configuration_data()
conf_data.set('BINDIR', get_option('prefix') / get_option('bindir'))
configure_file(
    input: 'xyz.openbmc_project.bios_bmc_smm_error_logger.service.in',
    output: 'xyz.openbmc_project.bios_bmc_smm_error_logger.service',
    configuration: conf_data,
    install: true,
    install_dir: systemd.get_variable('systemdsystemunitdir'),
)
