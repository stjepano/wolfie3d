#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/glad.h>

#include "common.h"
#include "surface.h"

// GLFW window size
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define WINDOW_TITLE "Wolfie3D"

// The size of the main texture
#define SURFACE_WIDTH 1024
#define SURFACE_HEIGHT 768

static const char *vertex_shader_src = "#version 330 core\n"
                                       "vec2 positions[3] = vec2[3](vec2(-1.0, 3.0), vec2(-1.0, -1.0), vec2(3.0, -1.0));\n"
                                       "vec2 uv_coords[3] = vec2[3](vec2(0.0, -1.0), vec2(0.0, 1.0), vec2(2.0, 1.0));\n"
                                       "out vec2 v_uv;\n"
                                       "void main() {\n"
                                       "    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);\n"
                                       "    v_uv = uv_coords[gl_VertexID];\n"
                                       "}\n";

static const char *fragment_shader_src = "#version 330 core\n"
                                         "in vec2 v_uv;\n"
                                         "out vec4 frag_color;\n"
                                         "uniform sampler2D u_texture;\n"
                                         "void main() {\n"
                                         "    frag_color = texture(u_texture, v_uv);\n"
                                         "}\n";

static GLFWwindow *s_window = nullptr;
static GLuint s_program = 0;
static GLuint s_vao = 0;
static GLuint s_texture = 0;

static Surface *s_surface = nullptr;
static Rect s_viewport = {0};
static float s_content_scale = 1.0f;
static float s_surface_scale_x = 1.0f;
static float s_surface_scale_y = 1.0f;

static int32_t line_a_set = 0;
static int32_t line_a[2] = {0};
static int32_t line_b_set = 0;
static int32_t line_b[2] = {0};

static void handle_framebuffer_resize(GLFWwindow *window, int width, int height)
{
    UNUSED(window);
    printf("Framebuffer resized: %dx%d\n", width, height);
    int w = MAX(width, 1);
    int h = MAX(height, 1);

    // NOTE: when moving to window with different scale, we should recalculate this in
    //       content scale callback (glfwSetWindowContentScaleCallback)
    // NOTE: glfwGetWindowContentScale is usually the exact ratio but on some platforms is
    //       rounded or stale so calculating it manually
    int window_width;
    glfwGetWindowSize(window, &window_width, nullptr);
    s_content_scale = (float)w / (float)window_width;

    int vpx, vpy, vpw, vph;
    if (w >= SURFACE_WIDTH && h >= SURFACE_HEIGHT)
    {
        // support only integer scaling to avoid artifacts (to be pixel perfect)
        int sw = w / SURFACE_WIDTH;
        int sh = h / SURFACE_HEIGHT;
        int iscale_factor = MIN(sw, sh);
        s_surface_scale_y = s_surface_scale_x = iscale_factor;

        vpw = SURFACE_WIDTH * iscale_factor;
        vph = SURFACE_HEIGHT * iscale_factor;

        vpx = (w - vpw) / 2;
        vpy = (h - vph) / 2;
    }
    else
    {
        // one of the framebuffer dimensions is smaller than the surface, we will scale the surface to fit the window while preserving the aspect ratio
        const float fb_aspect_ratio = (float)w / (float)h;
        const float surface_aspect_ratio = (float)SURFACE_WIDTH / (float)SURFACE_HEIGHT;
        vpw = w;
        vph = h;
        if (fb_aspect_ratio < surface_aspect_ratio)
        {
            vph = (int)((float)vpw / surface_aspect_ratio);
        }
        else
        {
            vpw = (int)((float)vph * surface_aspect_ratio);
        }
        vpx = (w - vpw) / 2;
        vpy = (h - vph) / 2;
        s_surface_scale_x = (float)vpw / (float)s_surface->width;
        s_surface_scale_y = (float)vph / (float)s_surface->height;
    }
    printf("Viewport: %dx%d at (%d, %d)\n", vpw, vph, vpx, vpy);
    glViewport(vpx, vpy, vpw, vph);
    // NOTE: viewport also stored top-down
    s_viewport = (Rect){vpx, h - vph - vpy, vpw, vph};
}

static void window_to_surface(float window_x, float window_y, int32_t *surface_x, int32_t *surface_y)
{
    assert(s_surface && s_surface->pixels);

    float framebuffer_x = window_x * s_content_scale;
    float framebuffer_y = window_y * s_content_scale;

    *surface_x = (int32_t)floorf((framebuffer_x - (float)s_viewport.x) / s_surface_scale_x);
    *surface_y = (int32_t)floorf((framebuffer_y - (float)s_viewport.y) / s_surface_scale_y);
}

static void handle_key_input(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    UNUSED(scancode);
    UNUSED(mods);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        printf("Escape key pressed, closing window.\n");
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static void handle_mouse_buttons(GLFWwindow *window, int button, int action, int mods)
{
    UNUSED(mods);
    if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS)
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        int32_t surface_x, surface_y;
        window_to_surface(x, y, &surface_x, &surface_y);
        printf("Left button pressed at %.6f, %.6f -> surface %d, %d\n", x, y, surface_x, surface_y);

        if (line_a_set && line_b_set)
        {
            line_a_set = 0;
            line_b_set = 0;
        }
        else
        {
            if (!line_a_set)
            {
                line_a_set = 1;
                line_a[0] = surface_x;
                line_a[1] = surface_y;
            }
            else
            {
                line_b_set = 1;
                line_b[0] = surface_x;
                line_b[1] = surface_y;
            }
        }
    }
}

static bool init_opengl()
{
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_src, nullptr);
    glCompileShader(vertex_shader);

    GLint success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info_log[512];
        glGetShaderInfoLog(vertex_shader, sizeof(info_log), nullptr, info_log);
        fprintf(stderr, "Vertex shader compilation failed:\n%s\n", info_log);
        glDeleteShader(vertex_shader);
        return false;
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_src, nullptr);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info_log[512];
        glGetShaderInfoLog(fragment_shader, sizeof(info_log), nullptr, info_log);
        fprintf(stderr, "Fragment shader compilation failed:\n%s\n", info_log);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }

    s_program = glCreateProgram();
    glAttachShader(s_program, vertex_shader);
    glAttachShader(s_program, fragment_shader);
    glLinkProgram(s_program);

    glGetProgramiv(s_program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char info_log[512];
        glGetProgramInfoLog(s_program, sizeof(info_log), nullptr, info_log);
        fprintf(stderr, "Shader program linking failed:\n%s\n", info_log);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(s_program);
        s_program = 0;
        return false;
    }

    // shaders are linked into the program and can be deleted
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint uniform_location = glGetUniformLocation(s_program, "u_texture");
    if (uniform_location == -1)
    {
        fprintf(stderr, "Failed to get uniform location for 'u_texture'\n");
        glDeleteProgram(s_program);
        s_program = 0;
        return false;
    }

    glGenVertexArrays(1, &s_vao);

    glBindVertexArray(s_vao);
    glUseProgram(s_program);

    glGenTextures(1, &s_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SURFACE_WIDTH, SURFACE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glUniform1i(uniform_location, 0); // Set the uniform to use texture unit 0

    return true;
}

static void cleanup_opengl()
{
    if (s_program)
    {
        glDeleteProgram(s_program);
        s_program = 0;
    }
    if (s_vao)
    {
        glDeleteVertexArrays(1, &s_vao);
        s_vao = 0;
    }
    if (s_texture)
    {
        glDeleteTextures(1, &s_texture);
        s_texture = 0;
    }
}

static void update_surface_texture()
{
    // For demonstration purposes -- simple checkerboard pattern
    for (int y = 0; y < SURFACE_HEIGHT; ++y)
    {
        bool y_even = ((y / 64) % 2) == 0;
        for (int x = 0; x < SURFACE_WIDTH; ++x)
        {
            bool x_even = ((x / 64) % 2) == 0;

            bool checkerboard = (x_even && y_even) || (!x_even && !y_even);

            uint32_t *pixel = ((uint32_t *)s_surface->pixels) + (y * s_surface->width) + x;
            *pixel = checkerboard ? encode_pixel((RGBA){255, 0, 0, 255}) : encode_pixel((RGBA){255, 255, 255, 255});
        }
    }

    if (line_a_set && line_b_set)
    {
        draw_line(s_surface, line_a[0], line_a[1], line_b[0], line_b[1], (RGBA){0, 0, 0, 255});
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SURFACE_WIDTH, SURFACE_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, s_surface->pixels);
}

int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    s_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (!s_window)
    {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(s_window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "Failed to initialize GLAD\n");
        glfwTerminate();
        return 1;
    }

    s_surface = allocate_surface(SURFACE_WIDTH, SURFACE_HEIGHT);
    if (!s_surface)
    {
        fprintf(stderr, "Failed to allocate memory for pixel data\n");
        glfwTerminate();
        return 1;
    }

    if (!init_opengl())
    {
        fprintf(stderr, "Failed to initialize OpenGL\n");
        free_surface(s_surface);
        cleanup_opengl();
        glfwTerminate();
        return 1;
    }

    GLfloat clear_color[] = {0.1f, 0.1f, 0.1f, 1.0f};

    {
        int fbw, fbh;
        glfwGetFramebufferSize(s_window, &fbw, &fbh);
        // inital nudge to set the viewport correctly
        handle_framebuffer_resize(s_window, fbw, fbh);
    }

    glfwSetFramebufferSizeCallback(s_window, handle_framebuffer_resize);
    glfwSetKeyCallback(s_window, handle_key_input);
    glfwSetMouseButtonCallback(s_window, handle_mouse_buttons);

    while (!glfwWindowShouldClose(s_window))
    {
        glfwPollEvents();

        glClearBufferfv(GL_COLOR, 0, clear_color);

        update_surface_texture();

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(s_window);
    }

    cleanup_opengl();
    free_surface(s_surface);
    glfwTerminate();
    return 0;
}