#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "types.hpp"
#include "patch.hpp"
#include "gradmesh.hpp"

std::vector<std::string> splitString(const std::string &str, char delimiter);
std::vector<Patch> readFile(const std::string &filename);