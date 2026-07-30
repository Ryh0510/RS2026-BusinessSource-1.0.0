#pragma once
#include <string>

class GLRuntime
{
public:
    static GLRuntime& instance();

    // This must be called after the OpenGL Context has already run `makeCurrent()`.
    bool initialize();

    void shutdown();

    bool isInitialized() const { return m_initialized; }

    int majorVersion() const { return m_major; }
    int minorVersion() const { return m_minor; }

    const std::string& vendor()   const { return m_vendor; }
    const std::string& renderer() const { return m_renderer; }
    const std::string& version()  const { return m_version; }
    const std::string& glsl()     const { return m_glsl; }

private:
    GLRuntime() = default;
    ~GLRuntime() = default;
    GLRuntime(const GLRuntime&) = delete;
    GLRuntime& operator=(const GLRuntime&) = delete;

    bool loadGLFunctions();
    void queryCapabilities();
    void setupDebugCallback();

private:
    bool m_initialized = false;
    int  m_major = 0;
    int  m_minor = 0;

    std::string m_vendor;
    std::string m_renderer;
    std::string m_version;
    std::string m_glsl;
};
