project(
    'glbg',
    'c',
    version: '0.0.1',
	license: 'MIT',
	meson_version: '>=1.1.0'
)

zeroterm = find_program('zeroterm')
link_bin = find_program('link_bin')

vert_glsl_null = custom_target(
    'vert.glsl_null',
    output: 'vert.glsl_null',
    input: 'vert.glsl',
    command: [zeroterm, '@INPUT@', '@OUTPUT@']
)

link_bin_gen = generator(
    link_bin,
    output: '@BASENAME@.o',
    arguments: ['@INPUT@', '@OUTPUT@']
)

shader_elfs = link_bin_gen.process(vert_glsl_null, 'frag_pref.glsl')

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols', version: '>=1.14')
wayland_scanner = dependency('wayland-scanner', version: '>=1.14.91', native: true)

wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')

wayland_scanner_prog = find_program(wayland_scanner.get_variable('wayland_scanner'), native: true)

wayland_scanner_code = generator(
    wayland_scanner_prog,
    output: '@BASENAME@-protocol.c',
    arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
    wayland_scanner_prog,
    output: '@BASENAME@-client-protocol.h',
    arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

protos_src = []

client_protocols = [
    wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
    'wlr-layer-shell-unstable-v1.xml',
]

foreach filename : client_protocols
    protos_src += wayland_scanner_code.process(filename)
    protos_src += wayland_scanner_client.process(filename)
endforeach

wayland_egl = dependency('wayland-egl')
egl = dependency('egl')
gl = dependency('gl')
deps = [wayland_client, wayland_egl, egl, gl]

includes = include_directories('include')

executable(
    'glbg',
    ['src/main.c', 'src/toml.c', 'src/render/shadertoy.c', protos_src],
    dependencies: deps,
    objects: shader_elfs,
    include_directories: includes,
    install: true
)

wayland_client = dependency('wayland-client')
