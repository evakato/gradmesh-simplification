#include "renderer.hpp"

GmsRenderer::GmsRenderer(GmsWindow &window, GmsGui &gui) : window{window}, gui{gui}
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    setupShaders();

    int maxTessellation;
    glGetIntegerv(GL_MAX_TESS_GEN_LEVEL, &maxTessellation);
    gui.maxHWTessellation = maxTessellation;

    setProjectionMatrix();
}

GmsRenderer::~GmsRenderer()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void GmsRenderer::setProjectionMatrix()
{
    projectionMatrix = glm::mat4(1.0f);
    float height = std::exp(GmsWindow::zoom);
    float width = 1.0f * height;
    float left = -width * 0.5f + GmsWindow::viewPos.x;
    float right = width * 0.5f + GmsWindow::viewPos.x;
    float bottom = height * 0.5f + GmsWindow::viewPos.y;
    float top = -height * 0.5f + GmsWindow::viewPos.y;

    projectionMatrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
}

const void GmsRenderer::setupShaders()
{
    std::vector curveShaders{
        ShaderSource{"../shaders/bezier.vs.glsl", GL_VERTEX_SHADER},
        ShaderSource{"../shaders/bezier.tcs.glsl", GL_TESS_CONTROL_SHADER},
        ShaderSource{"../shaders/bezier.tes.glsl", GL_TESS_EVALUATION_SHADER},
        ShaderSource{"../shaders/bezier.fs.glsl", GL_FRAGMENT_SHADER},
    };

    std::vector pointShaders{
        ShaderSource{"../shaders/controlpt.vs.glsl", GL_VERTEX_SHADER},
        ShaderSource{"../shaders/controlpt.fs.glsl", GL_FRAGMENT_SHADER},
    };

    std::vector lineShaders{
        ShaderSource{"../shaders/controlpt.vs.glsl", GL_VERTEX_SHADER},
        ShaderSource{"../shaders/line.fs.glsl", GL_FRAGMENT_SHADER},
    };

    curveShaderId = linkShaders(curveShaders);
    pointShaderId = linkShaders(pointShaders);
    lineShaderId = linkShaders(lineShaders);
}

const void GmsRenderer::startRenderFrame()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glLineWidth(gui.lineWidth);
}

const void GmsRenderer::setVertexData(std::vector<GLfloat> newData)
{
    vertexData = newData;
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(GLfloat), vertexData.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, coords));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, color));
    glEnableVertexAttribArray(1);
}

const void GmsRenderer::render()
{
    startRenderFrame();
    window.processInput();
    setProjectionMatrix();
}

const void GmsRenderer::highlightSelectedPoint(int numOfVerts)
{
    if (GmsWindow::validSelectedPoint())
    {
        int selectedIndex = GmsWindow::selectedPoint.primitiveId * numOfVerts + GmsWindow::selectedPoint.pointId;
        glUniform1i(glGetUniformLocation(pointShaderId, "selectedIndex"), selectedIndex);
    }
    else
    {
        glUniform1i(glGetUniformLocation(pointShaderId, "selectedIndex"), -1);
    }
}

void initializeOpenGL()
{
    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return;
    }

    glViewport(0, 0, GL_LENGTH, GL_LENGTH);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void setUniformProjectionMatrix(GLuint shaderId, glm::mat4 &projectionMatrix)
{
    GLint projectionLoc = glGetUniformLocation(shaderId, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projectionMatrix[0][0]);
}