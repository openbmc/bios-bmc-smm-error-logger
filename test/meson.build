gtest = dependency('gtest', main: true, disabler: true, required: false)
gmock = dependency('gmock', disabler: true, required: false)
if not gtest.found() or not gmock.found()
  gtest_opt = import('cmake').subproject_options()
  gtest_opt.append_compile_args('c++', ['-DCMAKE_CXX_FLAGS=-Wno-pedantic'])
  gtest_proj = cmake.subproject('googletest', options: gtest_opt, required: false)
  fmt_depj.dependency('fmt')

  if gtest_proj.found()
    gtest = declare_dependency(
      dependencies: [
        dependency('threads'),
        gtest_proj.dependency('gtest'),
        gtest_proj.dependency('gtest_main'),
      ])
    gmock = gtest_proj.dependency('gmock')
  endif
endif

test_dep = declare_dependency(
  include_directories: include_directories('include'),
  dependencies: [bios_bmc_smm_error_logger_dep, gtest, gmock, rde_dep]
)

gtests = [
  'pci_handler',
  'rde_dictionary_manager',
  'buffer',
  'external_storer_file',
  'rde_handler',
]
foreach t : gtests
  test(t, executable(t.underscorify(), t + '_test.cpp',
                     build_by_default: false,
                     implicit_include_directories: false,
                     dependencies: test_dep))
endforeach
