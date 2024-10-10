#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "types.hpp"
#include "patch.hpp"
#include "gms_appstate.hpp"
#include "gradmesh.hpp"

class GradMesh;

std::string extractFileName(const std::string &filepath);
std::vector<std::string> splitString(const std::string &str);
GradMesh readFile(const std::string &filename);
void writeLogFile(const GradMesh &mesh, const GmsAppState &state, const std::string &filename);