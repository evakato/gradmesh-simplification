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

std::vector<std::string> splitString(const std::string &str);
GradMesh readHemeshFile(const std::string &filename);
// GradMesh readCgmFile(const std::string &filename);
void writeHemeshFile(const std::string &filename, const GradMesh &mesh);
void writeLogFile(const GradMesh &mesh, const std::string &filename);

// Function to save the current framebuffer to a PNG file
void saveImage(const char *filename, int width, int height);

void createDir(const std::string_view dir);
void setupDirectories();

bool isValidNumber(const std::string &str);
int safeStringToInt(const std::string &str);

void saveConflictGraphToFile(const std::string &filename, const std::vector<TPRNode> &allTPRs, const std::vector<std::vector<int>> &adjList, const std::vector<EdgeRegion> &sortedRegions);
void loadConflictGraphFromFile(const std::string &filename, std::vector<TPRNode> &allTPRs, std::vector<std::vector<int>> &adjList, std::vector<EdgeRegion> &sortedRegions);