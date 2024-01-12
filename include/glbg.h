#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglplatform.h>

struct output;

struct renderer {
	void (*create)(struct output*);
	void (*render)(struct output*);
	void (*destroy)(struct output*);
};

struct config {
	char *name;
	void* renderer_create_data;
	const struct renderer* renderer;
	struct wl_list link;
};

struct output {
	struct state* state;
	struct wl_output *wl_output;
	struct wl_surface *surface;
	struct wl_egl_window *egl_window;
	EGLSurface egl_surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	char *name;
	uint32_t wl_name;
	uint32_t width;
	uint32_t height;
	uint32_t configure_serial;
	bool needs_ack;
	struct config *config;
	void *renderer_data;
	struct wl_list link;
};

struct state {
	struct wl_display *display;
	struct wl_registry *registry;

	struct wl_compositor *compositor;
	struct zwlr_layer_shell_v1 *layer_shell;

	struct output output;
	struct wl_list outputs;
	struct wl_list configs;
	EGLDisplay egl_display;
};

