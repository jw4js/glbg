#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <time.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>

#include "render/shadertoy.h"

struct shadertoy_renderer_data {
	uint64_t epoch;
	uint32_t shader_program;
    uint32_t VAO;
    uint32_t VBO;
    uint32_t EBO;

	int32_t uniform_iResolution;
	int32_t uniform_iTime;
};

static bool compile_shader(char* name, char* shader_src, uint32_t id) {
	glShaderSource(id, 1, (GLchar const**)&shader_src, NULL);
	glCompileShader(id);
	int32_t status;
	glGetShaderiv(id, GL_COMPILE_STATUS, &status);
	if(!status) {
		char buf[1024];
		glGetShaderInfoLog(id, sizeof(buf), NULL, buf);
		fprintf(stderr, "shader compilation failed: %s\n", buf);
		return false;
	}
	return true;
}

static uint64_t nanotime() {
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
	return tp.tv_sec * 1000000000ull + tp.tv_nsec;
}

static float get_iTime(uint64_t epoch) {
	uint64_t now = nanotime();
	uint64_t ms = (now - epoch) / 1000;
	return (double)ms / 1000000.0;
}

static void shadertoy_renderer_create(struct output* output) {
	const char *src_frag = output->config->renderer_create_data;
	struct shadertoy_renderer_data *data = calloc(1, sizeof(*data));

	data->epoch = nanotime();

	uint32_t vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	uint32_t fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	extern char _binary_vert_glsl_null_start[];
	compile_shader("vertex shader", _binary_vert_glsl_null_start, vertex_shader);

	extern char _binary_frag_pref_glsl_start[];
	extern char _binary_frag_pref_glsl_end[];
	size_t _binary_frag_pref_glsl_size = _binary_frag_pref_glsl_end - _binary_frag_pref_glsl_start;
	size_t src_frag_len = strlen(src_frag);

	char* src_frag_with_pref = malloc(src_frag_len + _binary_frag_pref_glsl_size + 1);
	memcpy(src_frag_with_pref, _binary_frag_pref_glsl_start, _binary_frag_pref_glsl_size);
	memcpy(src_frag_with_pref + _binary_frag_pref_glsl_size, src_frag, src_frag_len);
	*(src_frag_with_pref + src_frag_len + _binary_frag_pref_glsl_size) = 0;

	compile_shader("fragment shader", src_frag_with_pref, fragment_shader);
	free(src_frag_with_pref);

	data->shader_program = glCreateProgram();

	glAttachShader(data->shader_program, vertex_shader);
	glAttachShader(data->shader_program, fragment_shader);

	glLinkProgram(data->shader_program);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	int32_t status;
	glGetProgramiv(data->shader_program, GL_LINK_STATUS, &status);
	if(!status) {
		char buf[1024];
		glGetProgramInfoLog(data->shader_program, sizeof(buf), NULL, buf);
		fprintf(stderr, "failed to link shaders: %s\n", buf);
	}

	data->uniform_iResolution = glGetUniformLocation(data->shader_program, "iResolution");
	data->uniform_iTime = glGetUniformLocation(data->shader_program, "iTime");

	static const float vertices[] = {
		1.0f,  1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f 
    };
    static const uint32_t indices[] = {
    	0, 1, 3,
    	1, 2, 3,
    };

    glGenVertexArrays(1, &data->VAO);
    uint32_t bufs[2];
    glGenBuffers(2, bufs);
    data->VBO = bufs[0];
    data->EBO = bufs[1];

    glBindVertexArray(data->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, data->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data->EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    glBindVertexArray(0);

    output->renderer_data = data;
}

static void shadertoy_renderer_render(struct output* output) {
	struct shadertoy_renderer_data* data = output->renderer_data;

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(data->shader_program);

	glUniform3f(data->uniform_iResolution, output->width, output->height, 0);
	glUniform1f(data->uniform_iTime, get_iTime(data->epoch));

    glBindVertexArray(data->VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

static void shadertoy_renderer_destroy(struct output* output) {
	struct shadertoy_renderer_data* data = output->renderer_data;

	glDeleteProgram(data->shader_program);
	glDeleteVertexArrays(1, &data->VAO);
	uint32_t bufs[2] = {data->VBO, data->EBO};
	glDeleteBuffers(2, bufs);
}

const struct renderer shadertoy_renderer = {
	.create = shadertoy_renderer_create,
	.render = shadertoy_renderer_render,
	.destroy = shadertoy_renderer_destroy,
};

