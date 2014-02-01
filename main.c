#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <sys/select.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#define NSEC_PER_SEC        1000000000
#define TIME_T_MAX          (time_t)((1UL << ((sizeof(time_t) << 3) - 1)) - 1)

struct egl_device {
    EGLNativeDisplayType type;
    EGLDisplay display;
    EGLConfig config;
    EGLNativeWindowType window;
    EGLSurface surface;
    EGLContext context;
    const EGLint *attr_config;
    const EGLint *attr_context;
    int width;
    int height;
};

volatile sig_atomic_t done = 0;

void clock_gettime_diff(
    const struct timespec *time_beg,
    const struct timespec *time_end,
    struct timespec *diff)
{
    diff->tv_sec = time_end->tv_sec - time_beg->tv_sec;
    diff->tv_nsec = time_end->tv_nsec - time_beg->tv_nsec;
}

void clock_gettime_diff_add(
    const struct timespec *time_beg,
    const struct timespec *time_end,
    struct timespec *diff)
{
    diff->tv_sec += time_end->tv_sec - time_beg->tv_sec;
    diff->tv_nsec += time_end->tv_nsec - time_beg->tv_nsec;
}

long double clock_getdiff_nsec(const struct timespec *a, const struct timespec *b)
{
    return (b->tv_sec - a->tv_sec) * 1000000000.0L + (b->tv_nsec - a->tv_nsec);
}

long double clock_getdiff_sec(const struct timespec *a, const struct timespec *b)
{
    return (b->tv_sec - a->tv_sec) + (b->tv_nsec - a->tv_nsec) / 1000000000.0L;
}

/**
 * set_normalized_timespec - set timespec sec and nsec parts and normalize
 *
 * @ts:         pointer to timespec variable to be set
 * @sec:        seconds to set
 * @nsec:       nanoseconds to set
 *
 * Set seconds and nanoseconds field of a timespec variable and
 * normalize to the timespec storage format
 *
 * Note: The tv_nsec part is always in the range of
 *      0 <= tv_nsec < NSEC_PER_SEC
 * For negative values only the tv_sec field is negative !
 */
void set_normalized_timespec(struct timespec *ts, time_t sec, long long nsec)
{
    while (nsec >= NSEC_PER_SEC) {
        /*
         * The following asm() prevents the compiler from
         * optimising this loop into a modulo operation. See
         * also __iter_div_u64_rem() in include/linux/time.h
         */
        asm("" : "+rm"(nsec));
        nsec -= NSEC_PER_SEC;
        ++sec;
    }
    while (nsec < 0) {
        asm("" : "+rm"(nsec));
        nsec += NSEC_PER_SEC;
        --sec;
    }
    ts->tv_sec = sec;
    ts->tv_nsec = nsec;
}

/*
 * Add two timespec values and do a safety check for overflow.
 * It's assumed that both values are valid (>= 0)
 */
struct timespec timespec_add_safe(const struct timespec lhs,
                                  const struct timespec rhs)
{
    struct timespec res;

    set_normalized_timespec(&res, lhs.tv_sec + rhs.tv_sec,
                            lhs.tv_nsec + rhs.tv_nsec);

    if (res.tv_sec < lhs.tv_sec || res.tv_sec < rhs.tv_sec)
        res.tv_sec = TIME_T_MAX;

    return res;
}

/***************************************************************************************************
  Template platform

***************************************************************************************************/

//int egl_platform_get_display_type(struct egl_device *device)
//{
//    return 0;
//}

//int egl_platform_create_window(struct egl_device *device)
//{
//    return 0;
//}

//int egl_platform_destroy_window(struct egl_device *device)
//{
//    return 0;
//}

//void egl_platform_run(struct egl_device *device)
//{
//}

/**************************************************************************************************/

/***************************************************************************************************
  IMX6Q FB platform

***************************************************************************************************/

//int egl_platform_get_display_type(struct egl_device *device)
//{
//    device->type = (EGLNativeDisplayType)fbGetDisplayByIndex(0);
//    fbGetDisplayGeometry(device->type, &device->width, &device->height);

//    return 0;
//}

//int egl_platform_create_window(struct egl_device *device)
//{
//    device->window = (EGLNativeWindowType)fbCreateWindow(device->type, 0, 0, 0, 0);

//    return 0;
//}

//int egl_platform_destroy_window(struct egl_device *device)
//{
//    fbDestroyWindow(device->window);
//    device->window = (EGLNativeWindowType)0;

//    return 0;
//}

//void egl_platform_run(struct egl_device *device)
//{
//	setenv("FB_MULTIBUFFER", "2", 0);

//	int d = 0;
//	float i = 0.0;
//	while (!done)
//	{
// 		glClearColor(i, i, i, 1);
//        glClear(GL_COLOR_BUFFER_BIT);

//        eglSwapBuffers(device->display, device->surface);

//        if (d)
//            i += 0.01;
//        else
//            i -= 0.01;

//        if (i > 1.0 || i < 0.0) {
//            if (i > 1.0)
//                i = 1.0;
//            if (i < 0.0)
//                i = 0.0;

//            d = !d;
//        }
//    }

//	char cmd_line[1024];
//	sprintf(cmd_line, "fbset -xres %d -yres %d", device->width, device->height);
//	system(cmd_line);
//}

/**************************************************************************************************/

/***************************************************************************************************
  X11 platform

***************************************************************************************************/
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <pthread.h>

#define EGL_DEVICE_NAME     "Open GL ES 2"
#define EGL_DEVICE_X        0
#define EGL_DEVICE_Y        0
#define EGL_DEVICE_WIDTH    1280
#define EGL_DEVICE_HEIGHT   576

int egl_platform_get_display_type(struct egl_device *device)
{
    XInitThreads();

    device->type = (EGLNativeDisplayType)XOpenDisplay(0);

    device->width = EGL_DEVICE_WIDTH;
    device->height = EGL_DEVICE_HEIGHT;

    return 0;
}

int egl_platform_create_window(struct egl_device *device)
{
    static EGLint attr_config[] = {
        EGL_RED_SIZE,           1,
        EGL_GREEN_SIZE,         1,
        EGL_BLUE_SIZE,          1,
        EGL_DEPTH_SIZE,         1,
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    Window root = RootWindow(device->type, DefaultScreen(device->type));

    EGLConfig config;
    EGLint config_count;
    eglChooseConfig(device->display, attr_config, &config, 1, &config_count);
    assert(config);
    assert(config_count > 0);

    EGLint visual_id;
    eglGetConfigAttrib(device->display, config, EGL_NATIVE_VISUAL_ID, &visual_id);

    XVisualInfo visual_templ;
    visual_templ.visualid = visual_id;

    int visual_count;
    XVisualInfo *visual_info = XGetVisualInfo(device->type, VisualIDMask, &visual_templ, &visual_count);

    XSetWindowAttributes attr = { 0 };
    attr.colormap = XCreateColormap(device->type, root, visual_info->visual, AllocNone);
    attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;

    device->window = (EGLNativeWindowType)XCreateWindow(
        device->type,
        root,
        0,
        0,
        EGL_DEVICE_WIDTH,
        EGL_DEVICE_HEIGHT,
        0,
        visual_info->depth,
        InputOutput,
        visual_info->visual,
        CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
        &attr);

    XFree(visual_info);

    XSizeHints size_hints;
    size_hints.x = EGL_DEVICE_X;
    size_hints.y = EGL_DEVICE_Y;
    size_hints.width  = EGL_DEVICE_WIDTH;
    size_hints.height = EGL_DEVICE_HEIGHT;
    size_hints.flags = USSize | USPosition;

    XSetNormalHints(device->type, device->window, &size_hints);

    XSetStandardProperties(
        device->type,
        device->window,
        EGL_DEVICE_NAME,
        EGL_DEVICE_NAME,
        None,
        (char **)NULL,
        0,
        &size_hints);

    XMapWindow(device->type, device->window);

    return 0;
}

int egl_platform_destroy_window(struct egl_device *device)
{
    XDestroyWindow(device->type, device->window);
    XCloseDisplay(device->type);

    return 0;
}

static void *thread_start(void *arg)
{
    struct egl_device *device = (struct egl_device *)arg;

    int x11_fd = ConnectionNumber(device->type);
    fd_set in_fds;
    struct timeval timer;

    while (!done) {
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);

        timer.tv_usec = 0;
        timer.tv_sec = 1;

        select(x11_fd + 1, &in_fds, 0, 0, &timer);

        while (XPending(device->type)) {
            XEvent event;
            XNextEvent(device->type, &event);
            switch (event.type)
            {
                case Expose:
                    // Redraw
                    break;
                case ConfigureNotify:
                    device->width = event.xconfigure.width;
                    device->height = event.xconfigure.height;
                    break;
                case KeyPress:
                    {
                        XLookupKeysym(&event.xkey, 0);
                        char buf[10];
                        XLookupString(&event.xkey, buf, sizeof(buf), 0, 0);
                        if (buf[0] == 27)
                            done = 1;
                    }
                    break;
                default:
                    break;
            }
        }

    }

    return 0;
}

GLuint gl_compile_shader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    if (!shader)
        return -1;

    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLsizei length;
        GLchar info_log[65536];
        glGetShaderInfoLog(shader, 65536, &length, &info_log[0]);
        printf("%s\n", info_log);

        glDeleteShader(shader);
        return -1;
    }

    return shader;
}

GLuint gl_link_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLuint program = glCreateProgram();
    if (!program)
        return -1;

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);

    // Schedule shaders for later deletion
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(program);
        return -1;
    }

    glUseProgram(program);

    // Schedule program for later deletion
    glDeleteProgram(program);

    return program;
}

void egl_platform_run(struct egl_device *device)
{
    static const GLchar *vertex_shader_source[] = {
        "attribute vec4 v_position;         \n"
        "attribute vec2 v_coordinates;      \n"
        "varying vec2 coordinates;          \n"
        "                                   \n"
        "void main() {                      \n"
        "   coordinates = v_coordinates;    \n"
        "   gl_Position = v_position;       \n"
        "}                                  \n"
    };

    // YUV to BGR conversion matrix (column major)
    static const GLfloat matrix[] = {
        //   B        G        R
        1.000f,  1.000f,  1.000f, // Y
        1.772f, -0.344f,  0.000f, // V
        0.000f, -0.714f,  1.402f, // U
    };

    static const GLchar *fragment_shader_source[] = {
        "precision mediump float;                                   \n"
        "uniform sampler2D texture_y;                               \n"
        "uniform sampler2D texture_u;                               \n"
        "uniform sampler2D texture_v;                               \n"
        "varying vec2 coordinates;                                  \n"
        "uniform mat3 yuv_to_rgb;                                   \n"
        "                                                           \n"
        "void main() {                                              \n"
        "   float y = texture2D(texture_y, coordinates).r;          \n"
        "   float u = texture2D(texture_u, coordinates).r - 0.5;    \n"
        "   float v = texture2D(texture_v, coordinates).r - 0.5;    \n"
        "   vec3 rgb = yuv_to_rgb * vec3(y, u, v);                  \n"
        "   gl_FragColor = vec4(rgb, 1.0);                          \n"
        "}                                                          \n"
    };

    GLuint vertex_shader = gl_compile_shader(GL_VERTEX_SHADER, vertex_shader_source[0]);
    assert(vertex_shader != -1);
    GLuint fragment_shader = gl_compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source[0]);
    assert(fragment_shader != -1);
    GLuint program = gl_link_program(vertex_shader, fragment_shader);
    assert(program != -1);

    // -1, 1 (0) | 1, 1 (1)
    // ----------|---------
    // -1,-1 (2) | 1,-1 (3)
    GLfloat vertices[] = {
        -1,  1,  1,  1,
        -1, -1,  1, -1,
    };

    // 0, 0 (2) | 1, 0 (3)
    // ---------|---------
    // 0, 1 (0) | 1, 1 (1)
    GLfloat coordinates[] = {
        0, 0, 1, 0,
        0, 1, 1, 1,
    };

    GLint v_position = glGetAttribLocation(program, "v_position");
    glEnableVertexAttribArray(v_position);
    glVertexAttribPointer(v_position, 2, GL_FLOAT, GL_FALSE, 0, vertices);

    GLint v_coordinates = glGetAttribLocation(program, "v_coordinates");
    glEnableVertexAttribArray(v_coordinates);
    glVertexAttribPointer(v_coordinates, 2, GL_FLOAT, GL_FALSE, 0, coordinates);

    {
        int w = 1920, h = 1080; // frame.yuv dimensions
        int y = w * h;
        int v = y / 4;
        int u = v;

        FILE *fp = fopen("frame.yuv", "rb");

        fseek(fp, 0, SEEK_END);
        int file_size = ftell(fp);

        int frame_size = y + u + v;
        int frame_count = file_size / frame_size;

        printf("file_size = %d, frame_size = %d, frame_count = %d\n", file_size, frame_size, frame_count);

        GLvoid *planes[] = {
            (GLvoid *)malloc(y),
            (GLvoid *)malloc(u),
            (GLvoid *)malloc(v)
        };

        fseek(fp, file_size % frame_size, SEEK_SET);
        fread(planes[0], y, 1, fp);
        fread(planes[2], v, 1, fp);
        fread(planes[1], u, 1, fp);

        fclose(fp);

        glUniformMatrix3fv(
            glGetUniformLocation(program, "yuv_to_rgb"),
            1,
            GL_FALSE,
            matrix);

        GLuint textures[3];
        glGenTextures(3, textures);

        // Y
        GLint texture_y = glGetUniformLocation(program, "texture_y");
        glUniform1i(texture_y, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_y);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_LUMINANCE,
            w,
            h,
            0,
            GL_LUMINANCE,
            GL_UNSIGNED_BYTE,
            (GLvoid *)planes[0]);

        // U
        GLint texture_u = glGetUniformLocation(program, "texture_u");
        glUniform1i(texture_u, 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture_u);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_LUMINANCE,
            w / 2,
            h / 2,
            0,
            GL_LUMINANCE,
            GL_UNSIGNED_BYTE,
            (GLvoid *)planes[1]);

        // V
        GLint texture_v = glGetUniformLocation(program, "texture_v");
        glUniform1i(texture_v, 2);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, texture_v);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_LUMINANCE,
            w / 2,
            h / 2,
            0,
            GL_LUMINANCE,
            GL_UNSIGNED_BYTE,
            (GLvoid *)planes[2]);

        free(planes[2]);
        free(planes[1]);
        free(planes[0]);
    }

    glViewport(0, 0, device->width, device->height);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    system("setterm -cursor off");

    pthread_t thread;
    pthread_create(&thread, 0, &thread_start, (void *)device);

    int d = 0;
    float i = 0.0;
    int frame_count = 0;

    struct timespec time_slice = { 0 }, time_start, time_target, time_sync, time_loop_b, time_loop_e = { 0 };
    long elapsed = 0;

    time_slice.tv_nsec = 1 / 60.0 * 1000000000L;
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    time_sync = time_target = time_start;

    while (!done) {
        time_loop_b = time_loop_e;
        clock_gettime(CLOCK_MONOTONIC, &time_loop_e);
        time_target = time_loop_e;

        {
            glViewport(0, 0, device->width, device->height);

            glEnableVertexAttribArray(v_position);
            glEnableVertexAttribArray(v_coordinates);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            eglSwapBuffers(device->display, device->surface);

            if (d)
                i += 0.01;
            else
                i -= 0.01;

            if (i > 1.0 || i < 0.0) {
                if (i > 1.0)
                    i = 1.0;
                if (i < 0.0)
                    i = 0.0;

                d = !d;
            }
        }

        ++frame_count;

        fprintf(
            stdout,
            "F: %d, E: %.3Lf, C: %.3Lf       \r",
            frame_count,
            elapsed / 1000000000.0L,
            1 / (time_loop_e.tv_sec - time_loop_b.tv_sec + (time_loop_e.tv_nsec - time_loop_b.tv_nsec) / 1000000000.0L)
        );
        fflush(stdout);

        time_target = timespec_add_safe(time_target, time_slice);
        while (time_sync.tv_sec * 1000000000 + time_sync.tv_nsec < time_target.tv_sec * 1000000000 + time_target.tv_nsec) {
            clock_gettime(CLOCK_MONOTONIC, &time_sync);
            elapsed = (time_sync.tv_sec - time_start.tv_sec) * 1000000000.0L + (time_sync.tv_nsec - time_start.tv_nsec);
        }
    }

//    clock_gettime(CLOCK_MONOTONIC, &time_target);
//    printf(
//        "%.3Lf\n",
//        frame_count / (time_target.tv_sec - time_start.tv_sec) + (time_target.tv_nsec - time_start.tv_nsec) / 1000000000.0L
//    );

    printf("\n");

    pthread_join(thread, 0);

    system("setterm -cursor on");
}

/**************************************************************************************************/

int egl_initialize(struct egl_device *device)
{
    egl_platform_get_display_type(device);

    device->display = eglGetDisplay(device->type);
    assert(eglGetError() == EGL_SUCCESS);

    eglInitialize(device->display, NULL, NULL);
    assert(eglGetError() == EGL_SUCCESS);

    eglBindAPI(EGL_OPENGL_ES_API);
    assert(eglGetError() == EGL_SUCCESS);

    static EGLint attr_config[] = {
        EGL_SAMPLES,        0,
        EGL_RED_SIZE,       8,
        EGL_GREEN_SIZE,     8,
        EGL_BLUE_SIZE,      8,
        EGL_ALPHA_SIZE,     EGL_DONT_CARE,
        EGL_DEPTH_SIZE,     0,
        EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
        EGL_NONE
    };

    EGLint config_count = 0;
    eglChooseConfig(device->display, attr_config, &device->config, 1, &config_count);
    assert(eglGetError() == EGL_SUCCESS);
    assert(config_count == 1);

    device->attr_config = attr_config;

    egl_platform_create_window(device);

    device->surface = eglCreateWindowSurface(device->display, device->config, device->window, NULL);
    assert(eglGetError() == EGL_SUCCESS);

    static EGLint attr_context[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    device->context = eglCreateContext(device->display, device->config, EGL_NO_CONTEXT, attr_context);
    assert(eglGetError() == EGL_SUCCESS);

    device->attr_context = attr_context;

    eglMakeCurrent(device->display, device->surface, device->surface, device->context);
    assert(eglGetError() == EGL_SUCCESS);

    return 0;
}

int egl_uninitialize(struct egl_device *device)
{
    eglMakeCurrent(device->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    assert(eglGetError() == EGL_SUCCESS);

    eglDestroyContext(device->display, device->context);
    assert(eglGetError() == EGL_SUCCESS);
    device->context = (EGLContext)0;

    eglDestroySurface(device->display, device->surface);
    assert(eglGetError() == EGL_SUCCESS);
    device->surface = (EGLSurface)0;

    egl_platform_destroy_window(device);
    device->window = (EGLNativeWindowType)0;

    eglTerminate(device->display);
    assert(eglGetError() == EGL_SUCCESS);
    device->display = (EGLDisplay)0;

    eglReleaseThread();
    assert(eglGetError() == EGL_SUCCESS);

    return 0;
}

void signal_handler(int signal)
{
    done = 1;
}

void egl_run(struct egl_device *device)
{
    egl_platform_run(device);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, &signal_handler);
    signal(SIGTERM, &signal_handler);

    struct egl_device device;
    egl_initialize(&device);

    printf(
        "EGL vendor: %s\n"
        "EGL version: %s\n",
        eglQueryString(device.display, EGL_VENDOR),
        eglQueryString(device.display, EGL_VERSION));

    egl_run(&device);

    egl_uninitialize(&device);

    return 0;
}
