#include "fileio.hpp"

std::vector<std::string> splitString(const std::string &str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(str);

    while (std::getline(ss, token, delimiter))
    {
        tokens.push_back(token);
    }

    return tokens;
}
std::vector<Patch> readFile(const std::string &filename)
{
    std::ifstream inf{filename};
    if (!inf)
    {
        throw std::runtime_error("File could not be opened for reading\n");
    }

    std::string currLine{};
    std::vector<std::string> tokens;

    std::getline(inf, currLine);
    std::getline(inf, currLine);
    tokens = splitString(currLine, ' ');

    if (tokens.size() < 4)
    {
        throw std::runtime_error("Expected 4 tokens but got less than 4\n");
    }
    int numPoints = std::stoi(tokens[0]);
    int numHandles = std::stoi(tokens[1]);
    int numPatches = std::stoi(tokens[2]);
    int numEdges = std::stoi(tokens[3]);

    GradMesh gradMesh;

    for (size_t i = 0; i < numPoints; ++i)
    {
        if (std::getline(inf, currLine))
        {
            tokens = splitString(currLine, ' ');
            gradMesh.addPoint(std::stof(tokens[0]), std::stof(tokens[1]), std::stoi(tokens[2]));
        }
    }
    for (size_t i = 0; i < numHandles; ++i)
    {
        if (std::getline(inf, currLine))
        {
            tokens = splitString(currLine, ' ');
            gradMesh.addHandle(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5]), std::stoi(tokens[0]));
        }
    }
    for (size_t i = 0; i < numPatches; ++i)
    {
        if (std::getline(inf, currLine))
        {
            tokens = splitString(currLine, ' ');
            gradMesh.addFace(std::stoi(tokens[0]));
        }
    }
    for (size_t i = 0; i < numEdges; ++i)
    {
        if (std::getline(inf, currLine))
        {
            tokens = splitString(currLine, ' ');
            HalfEdge halfEdge;
            Twist twist{glm::vec2(std::stof(tokens[2]), std::stof(tokens[3])), glm::vec3(std::stof(tokens[4]), std::stof(tokens[5]), std::stof(tokens[6]))};
            halfEdge.twist = twist;
            halfEdge.color = glm::vec3(std::stof(tokens[7]), std::stof(tokens[8]), std::stof(tokens[9]));
            if (std::stoi(tokens[10]) >= 0)
            {
                halfEdge.handleIdxs = {std::stoi(tokens[10]), std::stoi(tokens[11])};
            }
            if (std::stoi(tokens[12]) >= 0)
                halfEdge.twinIdx = std::stoi(tokens[12]);
            halfEdge.prevIdx = std::stoi(tokens[13]);
            halfEdge.nextIdx = std::stoi(tokens[14]);
            halfEdge.patchIdx = std::stoi(tokens[15]);
            // if (std::stoi(tokens[16]) >= 0)
            // std::cout << "left most child type shi\n";
            if (std::stoi(tokens[17]))
            {
                halfEdge.parentIdx = std::stoi(tokens[18]);
                std::cout << i << " is child type shi whose parent is " << halfEdge.parentIdx << "\n";
                //  halfEdge.originIdx = gradMesh.getEdge(parentIdx).originIdx;
            }
            else
            {
                halfEdge.originIdx = std::stoi(tokens[20]);
                halfEdge.parentIdx = -1;
            }
            gradMesh.addEdge(halfEdge);
        }
    }
    gradMesh.setChildrenData();
    // std::cout << gradMesh;
    std::vector<std::vector<Vertex>> controlMatrices = gradMesh.getControlMatrix();
    std::vector<Patch> patches;
    for (auto cm : controlMatrices)
    {
        patches.push_back(Patch{cm});
    }
    return patches;
}