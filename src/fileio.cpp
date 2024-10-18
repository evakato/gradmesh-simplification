#include "fileio.hpp"

std::string extractFileName(const std::string &filepath)
{
    size_t lastSlash = filepath.find_last_of("/\\");
    size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
    size_t lastDot = filepath.find_last_of('.');
    if (lastDot == std::string::npos || lastDot < start)
    {
        return filepath.substr(start);
    }
    return filepath.substr(start, lastDot - start);
}

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

GradMesh readHemeshFile(const std::string &filename)
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
            halfEdge.twist = {glm::vec2(std::stof(tokens[2]), std::stof(tokens[3])), glm::vec3(std::stof(tokens[4]), std::stof(tokens[5]), std::stof(tokens[6]))};
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
            else
                halfEdge.twinIdx = -1;

            halfEdge.prevIdx = std::stoi(tokens[13]);
            halfEdge.nextIdx = std::stoi(tokens[14]);
            halfEdge.faceIdx = std::stoi(tokens[15]);
            // if (std::stoi(tokens[16]) >= 0)
            // std::cout << "left most child type shi\n";
            if (std::stoi(tokens[17]))
            {
                halfEdge.parentIdx = std::stoi(tokens[18]);
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
                halfEdge.originIdx = -1;
            }
            else
            {
                halfEdge.originIdx = std::stoi(tokens[20]);
                halfEdge.parentIdx = -1;
            }
            gradMesh.addEdge(halfEdge);
        }
    }

    gradMesh.fixEdges();
    //  td::cout << gradMesh;
    return std::move(gradMesh);
}

void writeHemeshFile(const std::string &filename, const GradMesh &mesh)
{
    std::ofstream out{filename};
    if (!out)
    {
        throw std::runtime_error("File could not be opened for writing\n");
    }

    const auto &points = mesh.getPoints();
    const auto &handles = mesh.getHandles();
    const auto &faces = mesh.getFaces();
    const auto &edges = mesh.getEdges();

    int numPoints = points.size();
    int numHandles = handles.size();
    int numPatches = faces.size();
    int numEdges = edges.size();

    out << "HEMESH\n";
    out << numPoints << " " << numHandles << " " << numPatches << " " << numEdges << "\n";

    for (const auto &point : points)
    {
        out << point.coords.x << " " << point.coords.y << " " << point.halfEdgeIdx << "\n";
    }

    for (const auto &handle : handles)
    {
        out << handle.halfEdgeIdx << " " << handle.coords.x << " " << handle.coords.y << " "
            << handle.color.x << " " << handle.color.y << " " << handle.color.z << "\n";
    }

    for (const auto &face : faces)
    {
        out << face.halfEdgeIdx << "\n";
    }

    for (auto &halfEdge : edges)
    {
        out << halfEdge.interval.x << " " << halfEdge.interval.y << " "                                        // interval (0,1)
            << halfEdge.twist.coords.x << " " << halfEdge.twist.coords.y << " "                                // twist coords (2,3)
            << halfEdge.twist.color.x << " " << halfEdge.twist.color.y << " " << halfEdge.twist.color.z << " " // twist color (4-6)
            << halfEdge.color.r << " " << halfEdge.color.g << " " << halfEdge.color.b << " "                   // color (7-9)
            << halfEdge.handleIdxs.first << " " << halfEdge.handleIdxs.second << " "                           // handle indices (10,11)
            << halfEdge.twinIdx << " "                                                                         // twin index (12)
            << halfEdge.prevIdx << " " << halfEdge.nextIdx << " " << halfEdge.faceIdx << " "                   // edge relations (13-15)
            << 0 << " "                                                                                        // placeholder (16)
            << (halfEdge.isChild() ? 1 : 0) << " "                                                             // child flag (17)
            << halfEdge.parentIdx << " "                                                                       // parent index (18)
            << 1 << " "                                                                                        // unused (19)
            << halfEdge.originIdx << "\n";                                                                     // origin index (20)
    }

    out.close();
}

void writeLogFile(const GradMesh &mesh, const GmsAppState &state, const std::string &filename)
{
    std::ofstream debugLogFile(std::string{LOGS_DIR} + "/" + filename);

    if (debugLogFile.is_open())
    {
        debugLogFile << "t: " << state.t << "\n";
        debugLogFile << "removed face id: " << state.removedFaceId << "\n";
        debugLogFile << "top edge: " << state.topEdgeCase << "\n";
        debugLogFile << state.topEdgeT << "\n";
        debugLogFile << "bottom edge: " << state.bottomEdgeCase << "\n";
        debugLogFile << state.bottomEdgeT << "\n";

        debugLogFile << mesh << std::endl;
        debugLogFile.close();
    }
    else
    {
        std::cerr << "Error opening file for writing!" << std::endl;
    }
}

void writeMergeList(const GmsAppState &state, const std::string &filename)
{
    if (state.merges.empty())
        return;

    std::ofstream debugLogFile(std::string{LOGS_DIR} + "/" + filename);
    if (debugLogFile.is_open())
    {
        for (auto merge : state.merges)
        {
            debugLogFile << merge << "\n";
        }
        debugLogFile.close();
    }
    else
    {
        std::cerr << "Error opening file for writing!" << std::endl;
    }
}

std::vector<int> readEdgeIdsFromFile(const std::string &filename)
{
    std::vector<int> edgeIds;
    std::ifstream file(filename);
    int value;
    if (!file.is_open())
    {
        std::cerr << "Error opening file: " << filename << std::endl;
        return edgeIds;
    }

    while (file >> value)
    {
        edgeIds.push_back(value);
    }
    file.close();
    return edgeIds;
}