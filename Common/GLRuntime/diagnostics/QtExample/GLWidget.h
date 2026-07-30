#pragma once
#include <QOpenGLWidget>
#include <QTimer>

class GLWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit GLWidget(QWidget* parent = nullptr);
    ~GLWidget();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void initShader();
    GLuint compile(GLenum type, const char* src);

private:
    GLuint vao = 0, vbo = 0, program = 0;
    float angle = 0.f;
    QTimer timer;
};
