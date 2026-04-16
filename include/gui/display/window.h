#pragma once

struct GLFWwindow;

namespace WindowUtils
{
    void maximize(GLFWwindow* window);

    GLFWwindow* createWindow();
}

