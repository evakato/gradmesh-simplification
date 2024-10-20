#pragma once

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "gradmesh.hpp"
#include "gms_appstate.hpp"
#include "patch.hpp"
#include "types.hpp"

class GradMesh;
class GmsAppState;

std::string extractFileName(const std::string &filepath);
std::vector<std::string> splitString(const std::string &str);
GradMesh readHemeshFile(const std::string &filename);
void writeHemeshFile(const std::string &filename, const GradMesh &mesh);
void writeLogFile(const GradMesh &mesh, const GmsAppState &state, const std::string &filename);
void writeMergeList(const GmsAppState &state, const std::string &filename);
std::vector<int> readEdgeIdsFromFile(const std::string &filename);