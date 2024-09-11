#include "window.hpp"

bool GmsWindow::isDragging = false;
bool GmsWindow::isClicked = false;
PointId GmsWindow::selectedPoint = {-1, -1};
glm::vec2 GmsWindow::mousePos = {0.0f, 0.0f};

double startX, startY;
double endX, endY;

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

    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
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
}

void GmsWindow::cursorPositionCallback(GLFWwindow *window, double xpos, double ypos)
{
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    if (ImGui::GetIO().WantCaptureMouse)
        return;

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

glm::vec2 GmsWindow::getNDCCoordinates(float screenX, float screenY)
{
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    float convertedX = screenX / viewport[2] * 2 - 1;
    float convertedY = -1 * (screenY / viewport[3] * 2 - 1);
    return glm::vec2(convertedX, convertedY);
}