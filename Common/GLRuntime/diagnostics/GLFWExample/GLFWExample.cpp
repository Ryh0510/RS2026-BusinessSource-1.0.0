#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <glad/glad.h>      // Must be first.
#include <GLFW/glfw3.h>

#include <GLRuntime/GLRuntime.h>
#include <cmath>
#include <iostream>

// ================= Shader =================

const char* vs = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 vColor;
uniform mat4 MVP;

void main()
{
    vColor = aColor;
    gl_Position = MVP * vec4(aPos, 1.0);
}
)";

const char* fs = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)";

// ================= Globals =================

GLuint vao, vbo, program;
float angleY = 0.0f;

// ================= Math (column-major) =================

void makeIdentity(float* m){ for(int i=0;i<16;i++) m[i]=(i%5==0)?1.f:0.f; }

void makeRotationY(float* m, float a){
    makeIdentity(m);
    m[0]=cos(a);  m[2]=-sin(a);
    m[8]=sin(a);  m[10]=cos(a);
}

void makeTranslate(float* m,float x,float y,float z){
    makeIdentity(m);
    m[12]=x; m[13]=y; m[14]=z;
}

void makePerspective(float* m,float fov,float asp,float n,float f){
    float t=tan(fov/2.f);
    for(int i=0;i<16;i++) m[i]=0;
    m[0]=1/(asp*t);
    m[5]=1/t;
    m[10]=-(f+n)/(f-n);
    m[11]=-1;
    m[14]=-(2*f*n)/(f-n);
    m[15] = 0; 

}

void mul(float* r, const float* a, const float* b)
{
    float t[16];
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            t[col * 4 + row] =
            a[0 * 4 + row] * b[col * 4 + 0] +
            a[1 * 4 + row] * b[col * 4 + 1] +
            a[2 * 4 + row] * b[col * 4 + 2] +
            a[3 * 4 + row] * b[col * 4 + 3];

    memcpy(r, t, sizeof(t));
}



// ================= Shader =================

void checkShader(GLuint s) {
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, 1024, nullptr, log);
        std::cout << "Shader error:\n" << log << "\n";
    }
}

void checkProgram(GLuint p) {
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, 1024, nullptr, log);
        std::cout << "Link error:\n" << log << "\n";
    }
}

GLuint compile(GLenum type, const char* src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);

    GLint ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(sh, 2048, nullptr, log);
        std::cout << "Shader compile error:\n" << log << "\n";
        exit(-1);
    }
    return sh;
}


void initShader(){
    GLuint v=compile(GL_VERTEX_SHADER,vs);
    GLuint f=compile(GL_FRAGMENT_SHADER,fs);
    program=glCreateProgram();

    checkShader(v);
    checkShader(f);

    glAttachShader(program,v);
    glAttachShader(program,f);

    glLinkProgram(program);
    checkProgram(program);

    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(program, 2048, nullptr, log);
        std::cout << "Program link error:\n" << log << "\n";
        exit(-1);
    }

    glDeleteShader(v);
    glDeleteShader(f);


}

// ================= Cube =================

float cube[] = {
    -1,-1,-1,1,0,0,  1,-1,-1,0,1,0,  1,1,-1,0,0,1,
    1,1,-1,0,0,1,  -1,1,-1,1,1,0, -1,-1,-1,1,0,0,
    -1,-1,1,1,0,1,  1,-1,1,0,1,1,  1,1,1,1,1,1,
    1,1,1,1,1,1,  -1,1,1,0,1,0, -1,-1,1,1,0,1,
    -1,1,1,1,0,0, -1,1,-1,0,1,0, -1,-1,-1,0,0,1,
    -1,-1,-1,0,0,1, -1,-1,1,1,1,0, -1,1,1,1,0,0,
    1,1,1,1,0,1, 1,1,-1,0,1,1, 1,-1,-1,1,1,1,
    1,-1,-1,1,1,1, 1,-1,1,0,1,0, 1,1,1,1,0,1,
    -1,-1,-1,1,0,0, 1,-1,-1,0,1,0, 1,-1,1,0,0,1,
    1,-1,1,0,0,1, -1,-1,1,1,1,0, -1,-1,-1,1,0,0,
    -1,1,-1,1,0,1, 1,1,-1,0,1,1, 1,1,1,1,1,1,
    1,1,1,1,1,1, -1,1,1,0,1,0, -1,1,-1,1,0,1
};

// ================= Init =================

void initGL(){
    if (!GLRuntime::instance().initialize()) {
        std::cout << "GL init failed\n";
        exit(-1);
    }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    initShader();

    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(cube),cube,GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    glEnable(GL_DEPTH_TEST);
}

// ================= Main =================

int main(){
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(800,600,"GLRuntime GLFW Demo",nullptr,nullptr);
    glfwMakeContextCurrent(win);

    initGL();

    std::cout << "GL Version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";


    while(!glfwWindowShouldClose(win)){
        int w,h; glfwGetFramebufferSize(win,&w,&h);
        glViewport(0,0,w,h);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

        angleY += 0.01f;

        float R[16],T[16],P[16],RT[16],MVP[16];
        makeRotationY(R,angleY);
        makeTranslate(T,0,0,-5);
        makePerspective(P,3.1416f/4,(float)w/h,0.1f,100.f);

        mul(RT,T,R);
        mul(MVP,P,RT);

        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program,"MVP"),1,GL_FALSE,MVP);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES,0,36);

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwTerminate();
}
