project('screensaver-service', 'c', version: '0.1.0', license: 'MIT', meson_version: '>= 0.60',
        default_options: ['c_std=gnu11', 'warning_level=2'])

add_project_arguments('-DVERSION="' + meson.project_version() + '"', language: 'c')

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
