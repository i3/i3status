# -*- mode: meson -*-

# Style objective: be consistent with what mesonbuild.com documents/uses, and/or
# the meson book: https://meson-manual.com/

project(
  'i3status',
  'c',
  version: '2.14',
  default_options: [
    'c_std=c11',
    'warning_level=1',  # enable all warnings (-Wall)
    # TODO(https://github.com/i3/i3/issues/4087): switch to
    # 'buildtype=debugoptimized',
  ],
  # Ubuntu 18.04 (supported until 2023) has meson 0.45.
  # We can revisit our minimum supported meson version
  # if it turns out to be too hard to maintain.
  meson_version: '>=0.45.0',
)

cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(['-Wunused-value']), language: 'c')

if meson.version().version_compare('>=0.48.0')
  # https://github.com/mesonbuild/meson/issues/2166#issuecomment-629696911
  meson.add_dist_script('meson/meson-dist-script')
else
  message('meson <0.48.0 detected, dist tarballs will not be filtered')
endif

################################################################################
# Version handling
################################################################################

cdata = configuration_data()

version_array = meson.project_version().split('.')
cdata.set_quoted('I3STATUS_VERSION', '@VCS_TAG@')
cdata.set_quoted('SYSCONFDIR', join_paths(get_option('prefix'), get_option('sysconfdir')))

if get_option('b_sanitize').split(',').contains('address')
  cdata.set('I3STATUS_ASAN_ENABLED', 1)
endif

cdata.set('HAVE_STRNDUP', cc.has_function('strndup'))
cdata.set('HAVE_MKDIRP', cc.has_function('mkdirp'))

if get_option('pulseaudio')
  cdata.set('HAS_PULSEAUDIO', 1)
endif

# Instead of generating config.h directly, make vcs_tag generate it so that
# @VCS_TAG@ is replaced.
config_h_in = configure_file(
  output: 'config.h.in',
  configuration: cdata,
)
config_h = declare_dependency(
  sources: vcs_tag(
    input: config_h_in,
    output: 'config.h',
    fallback: meson.project_version() + '-non-git',
  )
)

################################################################################
# manpages
################################################################################

man1 = join_paths(get_option('mandir'), 'man1')

if get_option('mans')
  asciidoc = find_program('asciidoc')
  asciidoc_cdata = configuration_data()
  asciidoc_cdata.set('PACKAGE_VERSION', meson.project_version())
  asciidoc_conf = configure_file(
    input: 'man/asciidoc.conf.in',
    output: 'asciidoc.conf',
    configuration: asciidoc_cdata,
  )

  xmlto = find_program('xmlto')

  pod2man = find_program('pod2man')

  man_inputs = [
    'man/i3status.man',
  ]

  foreach m : man_inputs
    xml = custom_target(
      m.underscorify()+'_asciidoc',
      input: m,
      output: '@BASENAME@.xml',
      command: [
        asciidoc,
        '-d', 'manpage',
        '-b', 'docbook',
        '-f', asciidoc_conf,
        '-o', '@OUTPUT@',
        '@INPUT@',
      ],
    )

    custom_target(
      m.underscorify()+'_xmlto',
      input: xml,
      output: '@BASENAME@.1',
      command: [
        xmlto,
        'man',
        '-o',
        '@OUTDIR@',
        '@INPUT@',
      ],
      # We should use install and install_dir instead of install_man as per:
      # https://github.com/mesonbuild/meson/issues/4981#issuecomment-467084867
      # https://github.com/mesonbuild/meson/issues/1550#issuecomment-370164307
      install: true,
      install_dir: man1,
    )
  endforeach

else
  if run_command('[', '-f', 'man/i3status.1', ']').returncode() == 0
    install_data(
      [
	'man/i3status.1',
      ],
      install_dir: man1,
    )
  endif
endif

if meson.version().version_compare('>=0.53')
  summary('build manpages (-Dmans)', get_option('mans'))
  summary('build pulseaudio support (-Dpulseaudio)', get_option('pulseaudio'))
endif

# Required for e.g. struct ucred to be defined as per unix(7).
add_project_arguments('-D_GNU_SOURCE', language: 'c')

# https://mesonbuild.com/howtox.html#add-math-library-lm-portably
m_dep = cc.find_library('m', required: false)
rt_dep = cc.find_library('rt', required: false)

confuse_dep = dependency('libconfuse', method: 'pkg-config')
yajl_dep = dependency('yajl', method: 'pkg-config')

i3status_srcs = [
  'i3status.c',
  'src/auto_detect_format.c',
  'src/first_network_device.c',
  'src/format_placeholders.c',
  'src/general.c',
  'src/output.c',
  'src/print_battery_info.c',
  'src/print_cpu_temperature.c',
  'src/print_cpu_usage.c',
  'src/print_ddate.c',
  'src/print_disk_info.c',
  'src/print_eth_info.c',
  'src/print_ip_addr.c',
  'src/print_ipv6_addr.c',
  'src/print_load.c',
  'src/print_mem.c',
  'src/print_path_exists.c',
  'src/print_run_watch.c',
  'src/print_time.c',
  'src/print_volume.c',
  'src/print_wireless_info.c',
  'src/print_file_contents.c',
  'src/process_runs.c',
]

thread_dep = dependency('threads')

i3status_deps = [
  thread_dep,
  m_dep,
  rt_dep,
  confuse_dep,
  yajl_dep,
  config_h,
]

if get_option('pulseaudio')
  pulse_dep = dependency('libpulse', method: 'pkg-config')
  i3status_deps += [pulse_dep]
  i3status_srcs += ['src/pulse.c']
endif

host_os = host_machine.system()
if host_os == 'linux'
  nlgenl_dep = dependency('libnl-genl-3.0', method: 'pkg-config')
  alsa_dep = dependency('alsa', method: 'pkg-config')
  i3status_deps += [nlgenl_dep, alsa_dep]
endif
if host_os == 'netbsd'
  prop_dep = cc.find_library('prop')
  i3status_deps += [prop_dep]
endif

inc = include_directories('include')

executable(
  'i3status',
  i3status_srcs,
  install: true,
  include_directories: inc,
  dependencies: i3status_deps,
)

install_subdir(
  'etc',
  strip_directory: true,
  install_dir: get_option('sysconfdir'),
)

# We cannot use configure_file for run-tests.pl.in
# because configure_file strips the backslash in e.g. \@display,
# resulting in @display, breaking our Perl code:
# https://github.com/mesonbuild/meson/issues/7165
sh = find_program('sh')
replace_dirs = [
  sh, '-c',  # Use bash to capture output and mark as executable
  'sed -e \'s,@abs_top_builddir@,'
  + meson.current_build_dir()
  + ',g;s,@abs_top_srcdir@,'
  + meson.current_source_dir()+',g\''
  # Only mark files ending in .pl as executables
  + ' "$0" > "$1" && { [ "${1##*.}" = pl ] && chmod +x "$1" || true; }',
  '@INPUT0@',   # $0
  '@OUTPUT0@',  # $1
]
run_tests = custom_target(
  'run-tests',
  input: ['travis/run-tests.pl.in'],
  output: ['run-tests.pl'],
  command: replace_dirs,
  # build this target when running e.g. ninja or ninja test.
  # This is required for older meson versions (< 0.46.0).
  build_by_default: true,
)

perl = find_program('perl')
if meson.version().version_compare('>=0.46.0')
  test(
    'run-tests',
    perl,
    args: [run_tests],
    timeout: 120,  # Default of 30 seconds can cause timeouts on slower machines
  )
else
  # meson < 0.46.0 does not support the depends arg in test targets.
  # Just hope for the best.
  test(
    'run-tests',
    perl,
    args: [run_tests],
  )
  message('meson < 0.46 detected, you might need to run ninja test twice')
endif
