project('screensaver-service', 'c', 'cpp', version: '0.1.0', license: 'MIT', meson_version: '>= 0.60',
        default_options: ['c_std=gnu11', 'cpp_std=c++17', 'warning_level=2'])

add_project_arguments('-DVERSION="' + meson.project_version() + '"', language: ['c', 'cpp'])

wayland_client = dependency('wayland-client')
x11 = dependency('x11')
xrandr = dependency('xrandr')

# The idle protocol comes from wayland-protocols; its C glue is generated here.
protocols = dependency('wayland-protocols', version: '>= 1.27')
protocol_dir = protocols.get_variable('pkgdatadir')
scanner = find_program('wayland-scanner')
idle_xml = protocol_dir / 'staging' / 'ext-idle-notify' / 'ext-idle-notify-v1.xml'
idle_h = custom_target('idle-header', input: idle_xml, output: 'ext-idle-notify-v1-client-protocol.h',
                       command: [scanner, 'client-header', '@INPUT@', '@OUTPUT@'])
idle_c = custom_target('idle-code', input: idle_xml, output: 'ext-idle-notify-v1-protocol.c',
                       command: [scanner, 'private-code', '@INPUT@', '@OUTPUT@'])

common = ['src/config.c', 'src/launch.c']
executable('screensaverd', ['src/daemon.c', idle_h, idle_c] + common, dependencies: [wayland_client, x11, xrandr], install: true)
executable('screensaverctl', ['src/ctl.c'] + common, dependencies: [x11, xrandr], install: true)

systemd_user = get_option('prefix') / 'lib' / 'systemd' / 'user'
install_data('data/screensaverd.service', install_dir: systemd_user)

# The settings window.
qt = import('qt6')
qt_dep = dependency('qt6', modules: ['Core', 'Gui', 'Widgets'])
moc = qt.compile_moc(headers: 'src/settings/panel.h', dependencies: qt_dep)
executable('screensaver-settings', ['src/settings/main.cpp', 'src/settings/panel.cpp', moc] + common,
           dependencies: [qt_dep, x11, xrandr], install: true)
install_data('data/screensaver-settings.desktop', install_dir: get_option('datadir') / 'applications')
install_data('data/io.github.mottipython.ScreensaverService.metainfo.xml', install_dir: get_option('datadir') / 'metainfo')

# Manual pages.
scdoc = find_program('scdoc', required: get_option('man'))
if scdoc.found()
  foreach page : ['screensaverd', 'screensaverctl', 'screensaver-settings']
    custom_target(page + '.1', input: 'man' / page + '.1.scd', output: page + '.1', feed: true, capture: true,
                  command: [scdoc], install: true, install_dir: get_option('mandir') / 'man1')
  endforeach
endif
