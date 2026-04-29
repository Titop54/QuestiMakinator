#pragma once

struct GLFWwindow;

namespace WindowUtils
{
    void maximize(GLFWwindow* window);

    GLFWwindow* createWindow();

    GLFWwindow* createHiddenContext(GLFWwindow* share = nullptr);

    void makeContextCurrent(GLFWwindow* window);

    void destroyWindow(GLFWwindow* window);

    void terminateGLFW();
}

