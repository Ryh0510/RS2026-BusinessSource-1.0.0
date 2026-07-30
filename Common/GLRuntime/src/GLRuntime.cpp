#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <glad/glad.h>
#include <GLRuntime/GLRuntime.h>
#include <iostream>



// ================= Debug Callback =================

static void APIENTRY glDebugOutput(GLenum, GLenum, GLuint,
                                   GLenum severity, GLsizei,
                                   const GLchar* message, const void*)
{
    if (severity == GL_DEBUG_SEVERITY_HIGH)
        std::cerr << "[GL ERROR] " << message << std::endl;
    else if (severity == GL_DEBUG_SEVERITY_MEDIUM)
        std::cerr << "[GL WARN ] " << message << std::endl;
}

// ================= Singleton =================

GLRuntime& GLRuntime::instance()
{
    static GLRuntime inst;
    return inst;
}

// ================= Initialization =================

bool GLRuntime::initialize()
{
    if (m_initialized)
        return true;

    if (!loadGLFunctions())
        return false;

    queryCapabilities();
    setupDebugCallback();

    m_initialized = true;
    return true;
}

void GLRuntime::shutdown()
{
    m_initialized = false;
}

// ================= GL Loader =================

bool GLRuntime::loadGLFunctions()
{
    if (!gladLoadGL())
    {
        std::cerr << "GLAD load failed! No current OpenGL context?" << std::endl;
        return false;
    }
    return true;
}

// ================= Capabilities =================

void GLRuntime::queryCapabilities()
{
    glGetIntegerv(GL_MAJOR_VERSION, &m_major);
    glGetIntegerv(GL_MINOR_VERSION, &m_minor);

    m_vendor   = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    m_renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    m_version  = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    m_glsl     = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    std::cout << "\n========== OpenGL Runtime ==========\n";
    std::cout << "Vendor   : " << m_vendor << "\n";
    std::cout << "Renderer : " << m_renderer << "\n";
    std::cout << "Version  : " << m_version << "\n";
    std::cout << "GLSL     : " << m_glsl << "\n";
    std::cout << "====================================\n";

    GLint maxUBO = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUBO);
    std::cout << "Max UBO Size: " << maxUBO << "\n";
}

// ================= Debug =================

void GLRuntime::setupDebugCallback()
{
#ifdef _DEBUG
    GLint flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);

    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glDebugOutput, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                              GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }
#endif
}
