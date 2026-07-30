#pragma once


#ifdef USING_GLEW
#include <GL/glew.h>
#include <GL/freeglut.h>
#endif // USING_GLEW


#ifdef USING_GLAD
#include <glad/glad.h>
#endif // USING_GLAD

#include <iostream>
using std::cout;
using std::endl;

#include <CustomLog/CustomLog.h>

class GLAdvanced
{
public:
	GLAdvanced(){
		CustomLog::init({
			{"rs2026",{true,true}}},
			"Robot", 
			"logs"
		);
	}
	~GLAdvanced(){}

	bool init(void) {
		//	1. Debug info
#ifdef _DEBUG
		LOG_DEBUG("rs2026") << "Called by GLAvanced::init()";
#endif

		//	2.	Using Glew
#ifdef USING_GLEW
		init_glew();
#endif // USING_GLEW


		//	3.	Using Glad
#ifdef USING_GLAD
		init_glad();
#endif // USING_GLAD

		return true;
}

private:
	
	//	Glew Initialization

#ifdef USING_GLEW
	bool init_glew(void) {
#ifdef _DEBUG
		LOG_DEBUG("rs2026") << "Called by glew::init()";
#endif

		GLenum err = glewInit();
		if (err) {
			LOG_DEBUG("rs2026") << "GLEW Initialization Error!";
			return false;
		}

#ifdef _DEBUG
		LOG_DEBUG("rs2026") << "glew::init(): glewInit() passed! ";
#endif


		//	1. Get GLSL OpenGL information.
		const GLubyte* renderer = glGetString(GL_RENDERER);
		const GLubyte* vendor = glGetString(GL_VENDOR);
		const GLubyte* version = glGetString(GL_VERSION);
		const GLubyte* glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);

		GLint major, minor;
		glGetIntegerv(GL_MAJOR_VERSION, &major);
		glGetIntegerv(GL_MINOR_VERSION, &minor);

		LOG_DEBUG("rs2026") << "GL Vendor    :" << vendor;
		LOG_DEBUG("rs2026") << "GL Renderer  : " << renderer;
		LOG_DEBUG("rs2026") << "GL Version (string)  : " << version;
		LOG_DEBUG("rs2026") << "GL Version (integer) : " << major << "." << minor;
		LOG_DEBUG("rs2026") << "GLSL Version : " << glslVersion;

		//2. Check the maximum block size and buffer bindings
		GLint max_block_size, max_binding_points;
		//GLint max_vertex_size, max_fragment_size;
		glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &max_block_size);
		glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_binding_points);
		//glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BUFFERS, &max_vertex_size);
		//glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BUFFERS, &max_fragment_size);

		LOG_DEBUG("rs2026") << "OpenGL Testing: (by Tang Qing)";
		LOG_DEBUG("rs2026") << "This computer offers maximum uniform block size: " << max_block_size;
		LOG_DEBUG("rs2026") << "This computer offers maximum buffer binding points :" << max_binding_points;

		return true;
	}
#endif // USING_GLEW



	//	Glad Initialization
#ifdef USING_GLAD
	bool init_glad(void) {
	
	// glad: load all OpenGL function pointers
	// ---------------------------------------
        if (!gladLoadGL())
        {
			LOG_WARNING("rs2026") << "Failed to initialize GLAD!";
            return false;
        }
		LOG_DEBUG("rs2026") << "OpenGL Version " << GLVersion.major << "." << GLVersion.minor << " loaded!";


        if (GLAD_GL_EXT_framebuffer_multisample) {
            /* GL_EXT_framebuffer_multisample is supported */
			LOG_DEBUG("rs2026") << "GL_EXT_framebuffer_multisample is supported!";
        }
        if (GLAD_GL_VERSION_3_0) {
            /* We support at least OpenGL version 3 */
			LOG_DEBUG("rs2026") << "We support at least OpenGL version 3!";
        }

#ifdef GLAD_DEBUG
        // before every opengl call call pre_gl_call
        glad_set_pre_callback(pre_gl_call);
        // don't use the callback for glClear
        // (glClear could be replaced with your own function)
        glad_debug_glClear = glad_glClear;
#endif

        return true;
	}
#endif // USING_GLAD

};




