# glbg

glbg is a tool which renders GL shader desktop backgrounds to wlroots-based compositors supporting a subset of the [Shadertoy](https://www.shadertoy.com/) API. It is compatible with any Wayland compositor which implements the wlr-layer-shell protocol and `wl_output` version 4.

## Demo

![Demo Final Flower](https://example.com)
![Demo Shadertoy Tutorial](https://example.com)

With a transparent background terminal, one can achieve the following effect. This also works with other windows which have transparency.

![Demo Transparent](https://example.com)

## Compiling

Install dependencies:

* meson \*
* wayland
* wayland-protocols \*
* EGL
* OpenGL-supporting grahpics driver

\* Compile time dependency

Run these commands:
    
    meson setup build
    ninja -C build
    sudo ninja -C build install

## Running

    glbg path/to/config.toml

## Configuration

The program is configured using a [TOML](https://toml.io/) file similar to the following:

    [[config]]
    name = "name of display"
    shader = "path/to/shader.glsl"

Note that the shader file is searched for relative to the location of the config file. To configure for multiple displays, simply duplicate the config block in the same file.

See the `example.toml` included in the repository.

## Supported Shadertoy uniforms

* `iTime`
* `iResolution`

