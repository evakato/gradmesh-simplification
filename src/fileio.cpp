#include "fileio.hpp"

std::vector<std::string> splitString(const std::string &str)
{
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(str);

    ss >> std::noskipws;
    char c;
    while (ss >> c)
    {
        if (!std::isspace(c))
        {
            token += c;
        }
        else if (!token.empty())
        {
            tokens.push_back(token);
            token.clear();
        }
    }

    if (!token.empty())
        tokens.push_back(token);

    return tokens;
}

GradMesh readFile(const std::string &filename)
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
    tokens = splitString(currLine);

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
            tokens = splitString(currLine);
            gradMesh.addPoint(std::stof(tokens[0]), std::stof(tokens[1]), std::stoi(tokens[2]));
        }
    }
    for (size_t i = 0; i < numHandles; ++i)
    {
        if (std::getline(inf, currLine))
        {
            tokens = splitString(currLine);
            gradMesh.addHandle(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5]), std::stoi(tokens[0]));
        }
    }
    for (size_t i = 0; i < numPatches; ++i)
    {
        if (std::getline(inf, currLine))
        {
            tokens = splitString(currLine);
            gradMesh.addFace(std::stoi(tokens[0]));
        }
    }
    for (size_t i = 0; i < numEdges; ++i)
    {
        if (std::getline(inf, currLine))
        {
            tokens = splitString(currLine);
            if (tokens.size() < 20)
            {
                throw std::runtime_error("Expected 20 tokens\n");
            }
            HalfEdge halfEdge;
            halfEdge.interval.x = std::stof(tokens[0]);
            halfEdge.interval.y = std::stof(tokens[1]);
            Twist twist{glm::vec2(std::stof(tokens[2]), std::stof(tokens[3])), glm::vec3(std::stof(tokens[4]), std::stof(tokens[5]), std::stof(tokens[6]))};
            halfEdge.twist = twist;
            halfEdge.color = glm::vec3(std::stof(tokens[7]), std::stof(tokens[8]), std::stof(tokens[9]));
            if (std::stoi(tokens[10]) >= 0)
            {
                halfEdge.handleIdxs = {std::stoi(tokens[10]), std::stoi(tokens[11])};
            }
            else
            {
                halfEdge.handleIdxs = {-1, -1};
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
                if (std::stoi(tokens[19]) < 0)
                {

                    std::cout << "edge " << i << ": " << tokens[18] << " is parent idx and patch idx is " << tokens[15] << "\n";
                }
                /*
                if (std::stoi(tokens[19]) >= 0)
                {
                    std::cout << "handles: \n";
                    std::cout << tokens[10] << ", " << tokens[11] << "\n";
                    std::cout << tokens[19] << ", " << tokens[20] << "\n";
                    std::cout << "interval: \n";
                    std::cout << tokens[0] << ", " << tokens[1] << "\n";
                    std::cout << tokens[21] << ", " << tokens[22] << "\n";
                }
                */
                // std::cout << i << " is child type shi whose parent is " << halfEdge.parentIdx << "\n";
                //   halfEdge.originIdx = gradMesh.getEdge(parentIdx).originIdx;
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
    return std::move(gradMesh);
}