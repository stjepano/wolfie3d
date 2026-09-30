#include <stdio.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/glad.h>

#define UNUSED(x) ((void)(x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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
                                         "void main() {\n"
                                         "    frag_color = vec4(v_uv, 0.0, 1.0);\n"
                                         "}\n";

static GLFWwindow *g_window = nullptr;
static GLuint g_program = 0;
static GLuint g_vao = 0;

static void handle_framebuffer_resize(GLFWwindow *window, int width, int height)
{
    UNUSED(window);
    printf("Framebuffer resized: %dx%d\n", width, height);
    int w = MAX(width, 1);
    int h = MAX(height, 1);

    int vpx, vpy, vpw, vph;
    if (w >= SURFACE_WIDTH && h >= SURFACE_HEIGHT)
    {
        // support only integer scaling to avoid artifacts (to be pixel perfect)
        int sw = w / SURFACE_WIDTH;
        int sh = h / SURFACE_HEIGHT;
        int iscale_factor = MIN(sw, sh);

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
    }
    printf("Viewport: %dx%d at (%d, %d)\n", vpw, vph, vpx, vpy);
    glViewport(vpx, vpy, vpw, vph);
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

    g_program = glCreateProgram();
    glAttachShader(g_program, vertex_shader);
    glAttachShader(g_program, fragment_shader);
    glLinkProgram(g_program);

    glGetProgramiv(g_program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char info_log[512];
        glGetProgramInfoLog(g_program, sizeof(info_log), nullptr, info_log);
        fprintf(stderr, "Shader program linking failed:\n%s\n", info_log);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(g_program);
        g_program = 0;
        return false;
    }

    // shaders are linked into the program and can be deleted
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glGenVertexArrays(1, &g_vao);

    glBindVertexArray(g_vao);
    glUseProgram(g_program);
    return true;
}

static void cleanup_opengl()
{
    if (g_program)
    {
        glDeleteProgram(g_program);
        g_program = 0;
    }
    if (g_vao)
    {
        glDeleteVertexArrays(1, &g_vao);
        g_vao = 0;
    }
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

    g_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (!g_window)
    {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "Failed to initialize GLAD\n");
        glfwTerminate();
        return 1;
    }

    if (!init_opengl())
    {
        fprintf(stderr, "Failed to initialize OpenGL\n");
        cleanup_opengl();
        glfwTerminate();
        return 1;
    }
    

    GLfloat clear_color[] = {0.1f, 0.1f, 0.1f, 1.0f};

    {
        int fbw, fbh;
        glfwGetFramebufferSize(g_window, &fbw, &fbh);
        // inital nudge to set the viewport correctly
        handle_framebuffer_resize(g_window, fbw, fbh);
    }

    glfwSetFramebufferSizeCallback(g_window, handle_framebuffer_resize);
    glfwSetKeyCallback(g_window, handle_key_input);

    while (!glfwWindowShouldClose(g_window))
    {
        glfwPollEvents();

        glClearBufferfv(GL_COLOR, 0, clear_color);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(g_window);
    }

    glDeleteVertexArrays(1, &g_vao);

    cleanup_opengl();
    glfwTerminate();
    return 0;
}