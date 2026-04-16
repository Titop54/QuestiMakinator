#include <glad/glad.h>
#include "gui/display/window.h"
#include <cstdlib>
#include <iostream>
#include <GLFW/glfw3.h>

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
        // Configuración para OpenGL moderno
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* window = glfwCreateWindow(800, 600, "QuestiMakinator", NULL, NULL);
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
}