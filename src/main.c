#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <libgen.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>

#include "toml.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include "render/shadertoy.h"
#include "glbg.h"

static void output_name(void *data, struct wl_output *wl_output, const char *name) {
	printf("output name: %s\n", name);
	struct output* output = data;
	struct config* config;
	output->name = strdup(name);
	wl_list_for_each(config, &output->state->configs, link) {
		if(strcmp(config->name, output->name) == 0) {
			printf("set config %s, %p, %p\n", name, output, config);
			output->config = config;
			break;
		}
	}
}

static void do_nothing(void) {
};

static void output_create_surface(struct wl_compositor* compositor, struct zwlr_layer_shell_v1 *layer_shell, struct output *output);

static void create_output(struct state* state, uint32_t name, struct wl_output* wl_output) {
	struct output* output = calloc(1, sizeof(*output));
	printf("new output %p\n", output);
	output->wl_name = name;
	output->wl_output = wl_output;
	// TODO: support size/mode/scale changes
	static const struct wl_output_listener output_listener = {
		.geometry = (void*)do_nothing,
		.mode = (void*)do_nothing,
		.done = (void*)do_nothing,
		.scale = (void*)do_nothing,
		.name = output_name,
		.description = (void*)do_nothing,
	};
	output->state = state;
	wl_output_add_listener(wl_output, &output_listener, output);
	output_create_surface(state->compositor, state->layer_shell, output);
	wl_list_insert(&state->outputs, &output->link);
}

static void destroy_output(struct output *output) {
	printf("destroy output %p\n", output);
	wl_list_remove(&output->link);
	if(output->config)
		output->config->renderer->destroy(output);
	if(output->egl_surface != EGL_NO_SURFACE)
		eglDestroySurface(output->state->egl_display, output->egl_surface);
	if(output->egl_window)
		wl_egl_window_destroy(output->egl_window);
	if(output->layer_surface != NULL)
		zwlr_layer_surface_v1_destroy(output->layer_surface);
	if(output->surface != NULL)
		wl_surface_destroy(output->surface);
	wl_output_destroy(output->wl_output);
	free(output->name);
	free(output);
}

static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct output *output = data;
	output->width = width;
	output->height = height;

	output->configure_serial = serial;
	output->needs_ack = true;
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1) {
	struct output *output = data;
	zwlr_layer_surface_v1_destroy(output->layer_surface);
	output->layer_surface = NULL;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void output_create_surface(struct wl_compositor* compositor, struct zwlr_layer_shell_v1 *layer_shell, struct output *output) {
	output->surface = wl_compositor_create_surface(compositor);
	struct wl_region *input_region = wl_compositor_create_region(compositor);
	wl_surface_set_input_region(output->surface, input_region);
	wl_region_destroy(input_region);

	output->layer_surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell, output->surface, output->wl_output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallpaper");

	zwlr_layer_surface_v1_set_size(output->layer_surface, 0, 0);
	zwlr_layer_surface_v1_set_anchor(output->layer_surface,
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
	zwlr_layer_surface_v1_set_exclusive_zone(output->layer_surface, -1);
	zwlr_layer_surface_v1_add_listener(output->layer_surface, &layer_surface_listener, output);
	wl_surface_commit(output->surface);
}

static void handle_global(void *data, struct wl_registry* registry, uint32_t name, const char *interface, uint32_t version) {
	struct state *state = data;
	if(strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	} else if(strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		state->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
	} else if(strcmp(interface, wl_output_interface.name) == 0) {
		struct wl_output* wl_output = wl_registry_bind(registry, name, &wl_output_interface, 4);
		create_output(state, name, wl_output);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	struct state *state = data;
	struct output *output, *tmp;
	printf("remove global\n");
	wl_list_for_each_safe(output, tmp, &state->outputs, link) {
		if (output->wl_name == name) {
			printf("remove global spec: %p\n", output);
			destroy_output(output);
			break;
		}
	}
}

char* read_file(char* filename) {
	FILE *file = fopen(filename, "r");
	if(!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	char* buf = malloc(size + 1);
	fseek(file, 0, SEEK_SET);
	fread(buf, 1, size, file);
	fclose(file);
	return buf;
}

bool parse_config(struct wl_list* configs, char* config_file) {
	char* dir = dirname(config_file);

	char* config_toml_str = read_file(config_file);
	if(!config_toml_str) {
		fprintf(stderr, "could not read config file: %s\n", config_file);
		return false;
	}

	const size_t BUF_SIZE = 256;
	char buf[BUF_SIZE];
	toml_table_t *tbl = toml_parse(config_toml_str, buf, BUF_SIZE);
	if(!tbl)
		goto err;
	toml_array_t *arr_configs = toml_table_array(tbl, "config");
	size_t n = toml_array_len(arr_configs);
	for(size_t i = 0; i < n; i++) {
		toml_table_t* tbl_config = toml_array_table(arr_configs, i);
		toml_value_t config_name = toml_table_string(tbl_config, "name");
		if(!config_name.ok) {
			fprintf(stderr, "\"name\" could not be read from config %zu\n", i);
			return false;
		}
		toml_value_t config_shader = toml_table_string(tbl_config, "shader");
		if(!config_shader.ok) {
			fprintf(stderr, "\"shader\" file name could not be read from config %zu\n", i);
			return false;
		}

		if(snprintf(buf, BUF_SIZE, "%s/%s", dir, config_shader.u.s) >= BUF_SIZE) {
			fprintf(stderr, "the shader file name %s is too long\n", config_shader.u.s);
			return false;
		}

		struct config* config = malloc(sizeof(*config));
		config->name = strdup(config_name.u.s);
		config->renderer = &shadertoy_renderer;
		config->renderer_create_data = read_file(config_shader.u.s);
		wl_list_insert(configs, &config->link);
	}
	return true;
err:
	fprintf(stderr, "failed to parse config: %s\n", buf);
	return false;
}

int main(int argc, char **argv) {
	struct state state;
	wl_list_init(&state.outputs);
	wl_list_init(&state.configs);

	state.display = wl_display_connect(NULL);
	if(!state.display) {
		fprintf(stderr, "failed to open wayland display\n");
		return 1;
	}

	state.registry = wl_display_get_registry(state.display);
	static const struct wl_registry_listener registry_listener = {
		.global = handle_global,
		.global_remove = handle_global_remove,
	};
	wl_registry_add_listener(state.registry, &registry_listener, &state);
	wl_display_roundtrip(state.display);

	state.egl_display = eglGetDisplay(state.display);
	
	EGLint egl_major;
	EGLint egl_minor;

	if(!eglInitialize(state.egl_display, &egl_major, &egl_minor)) {
		fprintf(stderr, "failed to initialize EGL\n");
		return 1;
	}

	printf("egl version %u %u\n", egl_major, egl_minor);

	eglBindAPI(EGL_OPENGL_API);

	GLint num_configs;

	if(!eglGetConfigs(state.egl_display, NULL, 0, &num_configs)) {
		fprintf(stderr, "failed to get configurations\n");
		return 2;
	}

	printf("found %u egl configs\n", num_configs);

	EGLint fb_attribs[] =
	{
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_RED_SIZE,        8,
		EGL_GREEN_SIZE,      8,
		EGL_BLUE_SIZE,       8,
		EGL_NONE
	};

	EGLConfig egl_config;

	// TODO: better config selection algorithm
	if(!eglChooseConfig(state.egl_display, fb_attribs, &egl_config, 1, &num_configs)) {
		fprintf(stderr, "failed to choose egl config\n");
		return 3;
	}

	EGLint egl_context_attribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
		EGL_NONE
	};

	EGLContext egl_context = eglCreateContext(state.egl_display, egl_config, EGL_NO_CONTEXT, egl_context_attribs);
	if(egl_context == EGL_NO_CONTEXT) {
		fprintf(stderr, "no EGL context\n");
		return 4;
	}

	if(argc != 2) {
		fprintf(stderr, "the program takes exactly one argument: the path to the config file\n");
		return 5;
	}

	if(!parse_config(&state.configs, argv[1]))
		return 6;

	while(wl_display_dispatch(state.display) != -1) {
		struct output* output;
		wl_list_for_each(output, &state.outputs, link) {
			if(output->needs_ack) {
				output->needs_ack = false;
				zwlr_layer_surface_v1_ack_configure(output->layer_surface, output->configure_serial);

				if(output->config && !output->renderer_data) {
					printf("width, height: %u %u\n", output->width, output->height);
					output->egl_window = wl_egl_window_create(output->surface, output->width, output->height);
					output->egl_surface = eglCreateWindowSurface(state.egl_display, egl_config, output->egl_window, NULL);
					eglMakeCurrent(state.egl_display, output->egl_surface, output->egl_surface, egl_context);
					printf("GL version: %s\n", glGetString(GL_VERSION));

					output->config->renderer->create(output);
    			}
			}

			if(output->renderer_data) {
				eglMakeCurrent(state.egl_display, output->egl_surface, output->egl_surface, egl_context);
				output->config->renderer->render(output);
        		eglSwapBuffers(state.egl_display, output->egl_surface);
        	}
        }
	}
}
