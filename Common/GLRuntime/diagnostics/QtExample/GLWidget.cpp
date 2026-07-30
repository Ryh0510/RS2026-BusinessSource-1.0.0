#include <glad/glad.h>
#include "GLWidget.h"
#include <GLRuntime/GLRuntime.h>
#include <QMatrix4x4>
#include <iostream>

// ================= Shader =================

static const char* vs = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
out vec3 vColor;
uniform mat4 MVP;
void main(){
    vColor = aColor;
    gl_Position = MVP * vec4(aPos, 1.0);
})";

static const char* fs = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main(){ FragColor = vec4(vColor, 1.0); }
)";

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

// ================= Qt GLWidget =================

GLWidget::GLWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    connect(&timer, &QTimer::timeout, this, [this]{
        angle += 1.f;
        update();
    });
    timer.start(16);
}

GLWidget::~GLWidget()
{
    makeCurrent();
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    doneCurrent();
}

void GLWidget::initializeGL()
{
    if (!GLRuntime::instance().initialize()) {
        std::cout << "GLRuntime init failed\n";
        exit(-1);
    }

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f,0.1f,0.15f,1.0f);

    initShader();

    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);

    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(cube),cube,GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
}

void GLWidget::resizeGL(int w, int h)
{
    glViewport(0,0,w,h);
}

void GLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);

    QMatrix4x4 model, view, proj;
    model.rotate(angle, 0,1,0);
    view.translate(0,0,-5);
    proj.perspective(45.f, float(width())/height(), 0.1f, 100.f);

    QMatrix4x4 mvp = proj * view * model;
    glUniformMatrix4fv(glGetUniformLocation(program,"MVP"),1,GL_FALSE,mvp.constData());

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES,0,36);
}

// ================= Shader helpers =================

GLuint GLWidget::compile(GLenum type,const char* src){
    GLuint s=glCreateShader(type);
    glShaderSource(s,1,&src,nullptr);
    glCompileShader(s);
    return s;
}

void GLWidget::initShader(){
    GLuint v=compile(GL_VERTEX_SHADER,vs);
    GLuint f=compile(GL_FRAGMENT_SHADER,fs);
    program=glCreateProgram();
    glAttachShader(program,v);
    glAttachShader(program,f);
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);
}
