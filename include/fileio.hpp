#pragma once

#include <cctype>
#include <filesystem>
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

std::string extractFileName(const std::string &filepath);
std::vector<std::string> splitString(const std::string &str);
GradMesh readHemeshFile(const std::string &filename);
void writeHemeshFile(const std::string &filename, const GradMesh &mesh);
void writeLogFile(const GradMesh &mesh, const std::string &filename);

// Function to save the current framebuffer to a PNG file
void saveImage(const char *filename, int width, int height);

void createDir(const std::string_view dir);
void setupDirectories();