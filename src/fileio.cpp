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

GradMesh readHemeshFile(const std::string &filename)
{
    // std::cout << "Reading " << filename << std::endl;
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

            assert(tokens.size() >= 20);

            HalfEdge halfEdge;
            halfEdge.interval.x = std::stof(tokens[0]);
            halfEdge.interval.y = std::stof(tokens[1]);
            halfEdge.twist = {glm::vec2(std::stof(tokens[2]), std::stof(tokens[3])), glm::vec3(std::stof(tokens[4]), std::stof(tokens[5]), std::stof(tokens[6]))};
            halfEdge.color = glm::vec3(std::stof(tokens[7]), std::stof(tokens[8]), std::stof(tokens[9]));
            if (safeStringToInt(tokens[10]) >= 0)
            {
                halfEdge.handleIdxs = {safeStringToInt(tokens[10]), safeStringToInt(tokens[11])};
            }
            else
            {
                halfEdge.handleIdxs = {-1, -1};
            }

            if (safeStringToInt(tokens[12]) >= 0)
                halfEdge.twinIdx = safeStringToInt(tokens[12]);
            else
                halfEdge.twinIdx = -1;

            halfEdge.prevIdx = safeStringToInt(tokens[13]);
            halfEdge.nextIdx = safeStringToInt(tokens[14]);
            halfEdge.faceIdx = safeStringToInt(tokens[15]);
            // if (std::stoi(tokens[16]) >= 0)
            // std::cout << "left most child type shi\n";
            if (safeStringToInt(tokens[17]))
            {
                halfEdge.parentIdx = safeStringToInt(tokens[18]);
                halfEdge.originIdx = -1;
            }
            else
            {
                halfEdge.parentIdx = -1;
                halfEdge.originIdx = safeStringToInt(tokens[20]);
            }

            if (tokens.size() > 23)
            {
                for (size_t i = 24; i < tokens.size(); ++i)
                {
                    halfEdge.childrenIdxs.push_back(safeStringToInt(tokens[i]));
                }
            }
            gradMesh.addEdge(halfEdge);
        }
    }
    gradMesh.fixEdges();

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
            << " merpity ";                                                                                    // placeholder (16)

        if (halfEdge.isChild())
        {
            out << 1 << " ";                                                              // 17
            out << halfEdge.parentIdx << " ";                                             // 18
            out << halfEdge.handleIdxs.first << " " << halfEdge.handleIdxs.second << " "; // 19, 20
            out << halfEdge.interval.x << " " << halfEdge.interval.y << " ";              // 21, 22
        }
        else
        {
            out << 0 << " ";                                                              // 17
            out << halfEdge.handleIdxs.first << " " << halfEdge.handleIdxs.second << " "; // 18, 19
            out << halfEdge.originIdx << " ";                                             // 20
            out << 0 << " " << 0 << " ";                                                  // 21, 22
        }
        out << 0 << " ";
        for (int childIdx : halfEdge.childrenIdxs)
        {
            out << childIdx << " "; // 24 +
        }
        out << "\n";
    }

    out.close();
}

void writeLogFile(const GradMesh &mesh, const std::string &filename)
{
    std::ofstream debugLogFile(std::string{LOGS_DIR} + "/" + filename);

    if (debugLogFile.is_open())
    {
        /*
        debugLogFile << "t: " << state.t << "\n";
        debugLogFile << "removed face id: " << state.removedFaceId << "\n";
        debugLogFile << "top edge: " << state.topEdgeCase << "\n";
        debugLogFile << state.topEdgeT << "\n";
        debugLogFile << "bottom edge: " << state.bottomEdgeCase << "\n";
        debugLogFile << state.bottomEdgeT << "\n";
        */

        debugLogFile << mesh << std::endl;
        debugLogFile.close();
    }
    else
    {
        std::cerr << "Error opening file for writing!" << std::endl;
    }
}

bool fileExists(const std::string &filename)
{
    std::ifstream file(filename);
    return file.good();
}

void saveImage(const char *filename, int width, int height)
{
    unsigned char *pixels = new unsigned char[width * height * 4];
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Flip the image vertically
    for (int y = 0; y < height / 2; ++y)
        for (int x = 0; x < width * 4; ++x)
            std::swap(pixels[y * width * 4 + x], pixels[(height - 1 - y) * width * 4 + x]);

    int stride_in_bytes = width * 4;

    // Check if the file already exists, if so, modify the filename
    std::string filepath = filename;
    std::string new_filepath = filepath;
    size_t extension_pos = filepath.find_last_of('.');
    if (extension_pos != std::string::npos)
    {
        std::string extension = filepath.substr(extension_pos);
        std::string base_filename = filepath.substr(0, extension_pos);

        int count = 1;
        while (fileExists(new_filepath))
        {
            new_filepath = base_filename + std::to_string(count++) + extension;
        }
    }

    // Save the image
    if (!stbi_write_png(new_filepath.c_str(), width, height, 4, pixels, stride_in_bytes))
    {
        std::cout << "Failed to save image!" << std::endl;
    }
    else
    {
        std::cout << "Image saved as " << new_filepath << std::endl;
    }

    delete[] pixels;
}

void createDir(const std::string_view dir)
{
    if (std::filesystem::exists(dir))
    {
        std::filesystem::remove_all(dir);
    }
    if (!std::filesystem::create_directory(dir))
        std::cout << "Failed to create directory!" << std::endl;
}

void setupDirectories()
{
    createDir(LOGS_DIR);
    createDir(IMAGE_DIR);
    createDir(SAVES_DIR);
    createDir(PREPROCESSING_DIR);
    if (!std::filesystem::exists(TPR_PREPROCESSING_DIR))
    {
        if (!std::filesystem::create_directory(TPR_PREPROCESSING_DIR))
            std::cout << "Failed to create directory!" << std::endl;
    }
}

bool isValidNumber(const std::string &str)
{
    if (str.empty())
    {
        return false;
    }

    size_t start = (str[0] == '-') ? 1 : 0;
    for (size_t i = start; i < str.size(); ++i)
    {
        if (!std::isdigit(str[i]))
        {
            return false;
        }
    }
    return true;
}

int safeStringToInt(const std::string &str)
{
    if (!isValidNumber(str))
    {
        // std::cerr << "Invalid input, defaulting to 0." << std::endl;
        return 0; // Default to 0 for invalid input
    }

    if (str.length() > std::to_string(std::numeric_limits<int>::max()).length())
    {
        // std::cerr << "Number too large, defaulting to 0." << std::endl;
        return 0; // Default to 0 for too large numbers
    }

    long long number = 0;
    size_t start = (str[0] == '-') ? 1 : 0; // Handle negative numbers

    for (size_t i = start; i < str.length(); ++i)
    {
        number = number * 10 + (str[i] - '0');
        if (number > std::numeric_limits<int>::max())
        {
            // std::cerr << "Number too large, defaulting to 0." << std::endl;
            return 0; // Default to 0 for overflow
        }
    }

    if (start == 1)
    {
        number = -number;
    }
    return static_cast<int>(number);
}

void saveConflictGraphToFile(const std::string &filename, const std::vector<TPRNode> &allTPRs, const std::vector<std::vector<int>> &adjList, const std::vector<EdgeRegion> &sortedRegions)
{
    std::ofstream outFile(filename);

    if (!outFile)
    {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    // Save allTPRs
    outFile << allTPRs.size() << "\n";
    for (const auto &node : allTPRs)
    {
        outFile << node.id << " "
                << node.gridPair.first << " " << node.gridPair.second << " "
                << node.maxRegion.first << " " << node.maxRegion.second << " "
                << std::fixed << std::setprecision(10) << node.error << " "
                << node.maxChainLength << " "
                << node.degree << "\n";
    }

    // Save adjList
    outFile << adjList.size() << "\n";
    for (const auto &adj : adjList)
    {
        outFile << adj.size();
        for (int node : adj)
        {
            outFile << " " << node;
        }
        outFile << "\n";
    }

    // Save sortedRegions
    outFile << sortedRegions.size() << "\n";
    for (const auto &edgeRegion : sortedRegions)
    {
        outFile << edgeRegion.gridPair.first << " " << edgeRegion.gridPair.second << " "
                << edgeRegion.faceIdx << "\n";
        outFile << edgeRegion.allRegionAttributes.size() << "\n";
        for (const auto &region : edgeRegion.allRegionAttributes)
        {
            outFile << region.maxRegion.first << " " << region.maxRegion.second << " "
                    << region.error << " "
                    << region.maxChainLength << "\n";
            outFile << region.maxRegionAABB.min.x << " " << region.maxRegionAABB.min.y << " "
                    << region.maxRegionAABB.max.x << " " << region.maxRegionAABB.max.y << "\n";
        }
    }

    outFile.close();
    std::cout << "Data saved to " << filename << std::endl;
}

// Load function to read from file

void loadConflictGraphFromFile(const std::string &filename, std::vector<TPRNode> &allTPRs, std::vector<std::vector<int>> &adjList, std::vector<EdgeRegion> &sortedRegions)
{
    std::ifstream inFile(filename);

    if (!inFile)
    {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
        return;
    }

    // Load allTPRs
    size_t numTPRs;
    inFile >> numTPRs;
    allTPRs.resize(numTPRs);

    for (auto &node : allTPRs)
    {
        inFile >> node.id >> node.gridPair.first >> node.gridPair.second >> node.maxRegion.first >> node.maxRegion.second >> node.error >> node.maxChainLength >> node.degree;
    }

    // Load adjList
    size_t numAdjList;
    inFile >> numAdjList;
    adjList.resize(numAdjList);

    for (auto &adj : adjList)
    {
        size_t numAdjNodes;
        inFile >> numAdjNodes;
        adj.resize(numAdjNodes);

        for (int &node : adj)
        {
            inFile >> node;
        }
    }

    // Load sortedRegions
    size_t numSortedRegions;
    inFile >> numSortedRegions;
    sortedRegions.resize(numSortedRegions);

    for (auto &edgeRegion : sortedRegions)
    {
        inFile >> edgeRegion.gridPair.first >> edgeRegion.gridPair.second >> edgeRegion.faceIdx;

        size_t numRegionAttributes;
        inFile >> numRegionAttributes;
        edgeRegion.allRegionAttributes.resize(numRegionAttributes);

        for (auto &region : edgeRegion.allRegionAttributes)
        {
            inFile >> region.maxRegion.first >> region.maxRegion.second >> region.error >> region.maxChainLength;

            inFile >> region.maxRegionAABB.min.x >> region.maxRegionAABB.min.y >> region.maxRegionAABB.max.x >> region.maxRegionAABB.max.y;
        }
    }

    inFile.close();
    std::cout << "Data loaded from " << filename << std::endl;
}
