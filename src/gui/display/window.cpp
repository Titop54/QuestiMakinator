#include "gui/display/window.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

namespace WindowUtils
{
    void glfw_error_callback(int error, const char* description)
    {
        std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
    }
    
    void maximize(GLFWwindow* window)
    {
        if(window)
        {
            glfwMaximizeWindow(window);
        }
    }

    GLFWwindow* createWindow()
    {
        glfwSetErrorCallback(glfw_error_callback);
        if(!glfwInit())
        {
            std::cerr << "Failed to initialize GLFW\n";
            exit(-1);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* window = glfwCreateWindow(800, 600, "QuestiMakinator", nullptr, nullptr);
        if(!window)
        {
            glfwTerminate();
            return nullptr;
        }

        glfwMakeContextCurrent(window);

        if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return nullptr;
        }
        
        glfwSwapInterval(1); // Enable vsync
        
        maximize(window);
        return window;
    }

    GLFWwindow* createHiddenContext(GLFWwindow* share)
    {
        if(!glfwInit())
        {
            std::cerr << "GLFW not initialized before creating hidden context\n";
            return nullptr;
        }

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* hidden = glfwCreateWindow(1, 1, "Hidden Context", nullptr, share);
        if(!hidden)
        {
            std::cerr << "Failed to create hidden GLFW window for worker thread\n";
            return nullptr;
        }

        glfwMakeContextCurrent(hidden);
        if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cerr << "Failed to initialize GLAD on hidden context\n";
            glfwDestroyWindow(hidden);
            return nullptr;
        }

        glfwSwapInterval(0);
        return hidden;
    }

    void makeContextCurrent(GLFWwindow* window)
    {
        if(window) glfwMakeContextCurrent(window);
    }

    void destroyWindow(GLFWwindow* window)
    {
        if(window)
        {
            glfwDestroyWindow(window);
        }
    }

    void terminateGLFW()
    {
        glfwTerminate();
    }
}