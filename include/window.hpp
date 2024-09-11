#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "imgui_impl_glfw.h"

#include "curve.hpp"
#include "patch.hpp"
#include "types.hpp"

#include <iostream>

class GmsWindow {
    public:
        GmsWindow(int width, int height, std::string name);
        ~GmsWindow();

        GLFWwindow* getGLFWwindow() const { return window; }
        void processInput();
        bool shouldClose() { return glfwWindowShouldClose(window); }
        void swapBuffers() { glfwSwapBuffers(window); }

        static bool const validSelectedPoint() {
            if (selectedPoint.primitiveId != -1 && selectedPoint.pointId != -1) return true;
            return false;
        }
        static glm::vec2 getNDCCoordinates(float screenX, float screenY);

        static PointId selectedPoint;
        static glm::vec2 mousePos;
        static bool isDragging;
        static bool isClicked;

    private: 
        void initWindow();

        int width;
        int height;
        std::string name;
        GLFWwindow* window;

        // callback functions
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
            glViewport(0, 0, width, height);
        }
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);

};