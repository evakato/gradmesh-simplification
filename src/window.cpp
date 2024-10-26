#include "window.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool GmsWindow::isDragging = false;
bool GmsWindow::isClicked = false;
PointId GmsWindow::selectedPoint = {-1, -1};
glm::vec2 GmsWindow::mousePos = {0.0f, 0.0f};
float GmsWindow::zoom = 0.5f;
glm::vec2 GmsWindow::viewPos{0, 0};

double startX, startY;
double endX, endY;
bool middleMouseButtonPressed = false;

GmsWindow::GmsWindow(int width, int height, std::string name) : width(width), height(height), name(name)
{
    initWindow();
}

GmsWindow::~GmsWindow()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

void GmsWindow::initWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(width, height, name.c_str(), NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void GmsWindow::processInput()
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
        GmsWindow::zoom -= 0.15f;
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
        GmsWindow::zoom += 0.15f;

    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);
}

void GmsWindow::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_S && (mods & GLFW_MOD_CONTROL))
        {
            std::cout << "Ctrl + S was pressed\n";
        }
        if (key == GLFW_KEY_O && (mods & GLFW_MOD_CONTROL))
        {
            std::cout << "Ctrl + O was pressed\n";
        }
    }
}

void GmsWindow::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        glfwGetCursorPos(window, &startX, &startY);
        mousePos = getNDCCoordinates(startX, startY);
        isDragging = false;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        glfwGetCursorPos(window, &endX, &endY);

        double deltaX = endX - startX;
        double deltaY = endY - startY;
        double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);

        if (distance < 2.0)
            isClicked = true;
        else
            isClicked = false;

        // selectedPoint.primitiveId = -1;
        // selectedPoint.pointId = -1;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        if (action == GLFW_PRESS)
        {
            middleMouseButtonPressed = true;
            glfwGetCursorPos(window, &startX, &startY);
            mousePos = getNDCCoordinates(startX, startY);
        }
        else if (action == GLFW_RELEASE)
        {
            middleMouseButtonPressed = false;
        }
    }
}

void GmsWindow::cursorPositionCallback(GLFWwindow *window, double xpos, double ypos)
{
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    if (middleMouseButtonPressed)
    {
        double deltaX = xpos - startX;
        double deltaY = ypos - startY;
        viewPos.x -= 0.001f * deltaX;
        viewPos.y -= 0.001f * deltaY;
        startX = xpos;
        startY = ypos;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        double deltaX = xpos - startX;
        double deltaY = ypos - startY;
        double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);

        if (distance > 2.0)
        {
            isDragging = true;
            if (validSelectedPoint())
                mousePos = getNDCCoordinates(xpos, ypos);
            // std::cout << "Dragging at position: (" << xpos << ", " << ypos << ")" << std::endl;
        }
    }
}

void GmsWindow::scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
    {
        // If ImGui wants to capture mouse input, let it handle the scroll
        ImGui::GetIO().MouseWheel += (float)yoffset;
    }
    else
    {
        // std::cout << "Scroll X offset: " << xoffset << " Scroll Y offset: " << yoffset << std::endl;
        GmsWindow::zoom += -1.0f * yoffset * 0.15f;
    }
}

glm::vec2 GmsWindow::getNDCCoordinates(float screenX, float screenY)
{
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    float convertedX = screenX / viewport[2] * 2 - 1;
    float convertedY = -1 * (screenY / viewport[3] * 2 - 1);
    return glm::vec2(convertedX, convertedY);
}
