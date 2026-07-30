#include <glad/glad.h>
#include <GL/freeglut.h>
#include <GLRuntime/GLRuntime.h>
#include <cmath>

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

// ================= Math =================

void makeIdentity(float* m)
{
    for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.f : 0.f;
}

void makeRotationY(float* m, float a)
{
    makeIdentity(m);
    m[0] = cos(a);
    m[2] = -sin(a);
    m[8] = sin(a);
    m[10] = cos(a);
}

void makeTranslate(float* m, float x, float y, float z)
{
    makeIdentity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

void makePerspective(float* m, float fov, float asp, float n, float f)
{
    float t = tan(fov / 2.f);
    for (int i = 0; i < 16; i++) m[i] = 0;
    m[0] = 1 / (asp * t);
    m[5] = 1 / t;
    m[10] = -(f + n) / (f - n);
    m[11] = -1;
    m[14] = -(2 * f * n) / (f - n);
}

void mul(float* r, const float* a, const float* b)
{
    float t[16];
    for (int c = 0; c < 4; ++c)
        for (int r_ = 0; r_ < 4; ++r_) {
            t[c * 4 + r_] =
                a[0 * 4 + r_] * b[c * 4 + 0] +
                a[1 * 4 + r_] * b[c * 4 + 1] +
                a[2 * 4 + r_] * b[c * 4 + 2] +
                a[3 * 4 + r_] * b[c * 4 + 3];
        }
    memcpy(r, t, sizeof(float) * 16);
}


// ================= Shader Helpers =================

GLuint compile(GLenum t, const char* s)
{
    GLuint sh = glCreateShader(t);
    glShaderSource(sh, 1, &s, nullptr);
    glCompileShader(sh);
    return sh;
}

void initShader()
{
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);
}

// ================= Cube Data =================

float cube[] = {
    // pos            // color
   -1,-1,-1, 1,0,0,  1,-1,-1, 0,1,0,  1,1,-1, 0,0,1,
    1,1,-1, 0,0,1, -1,1,-1, 1,1,0, -1,-1,-1, 1,0,0,

   -1,-1,1, 1,0,1,  1,-1,1, 0,1,1,  1,1,1, 1,1,1,
    1,1,1, 1,1,1, -1,1,1, 0,1,0, -1,-1,1, 1,0,1,

   -1,1,1, 1,0,0, -1,1,-1, 0,1,0, -1,-1,-1, 0,0,1,
   -1,-1,-1, 0,0,1, -1,-1,1, 1,1,0, -1,1,1, 1,0,0,

    1,1,1, 1,0,1,  1,1,-1, 0,1,1,  1,-1,-1, 1,1,1,
    1,-1,-1, 1,1,1,  1,-1,1, 0,1,0,  1,1,1, 1,0,1,

   -1,-1,-1, 1,0,0,  1,-1,-1, 0,1,0,  1,-1,1, 0,0,1,
    1,-1,1, 0,0,1, -1,-1,1, 1,1,0, -1,-1,-1, 1,0,0,

   -1,1,-1, 1,0,1,  1,1,-1, 0,1,1,  1,1,1, 1,1,1,
    1,1,1, 1,1,1, -1,1,1, 0,1,0, -1,1,-1, 1,0,1
};

// ================= Init =================

void initGL()
{
    GLRuntime::instance().initialize();

    initShader();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glEnable(GL_DEPTH_TEST);
}

// ================= Render =================

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);

    float R[16], T[16], P[16], RT[16], MVP[16];

    makeRotationY(R, angleY);
    makeTranslate(T, 0, -2, -10);
    makePerspective(P, 3.1416f / 4, 800.f / 600.f, 0.1f, 100.f);

    mul(RT, T, R);     // T * R
    mul(MVP, P, RT);   // P * T * R


    glUniformMatrix4fv(glGetUniformLocation(program, "MVP"), 1, GL_FALSE, MVP);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glutSwapBuffers();
}

void idle()
{
    angleY += 0.01f;
    glutPostRedisplay();
}

// ================= Main =================

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("GLRuntime FreeGLUT Demo");

    initGL();

    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutMainLoop();
}
