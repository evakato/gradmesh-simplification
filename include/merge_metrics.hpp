#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>

#include <rmgr/ssim.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image_write.h"

#include "patch.hpp"
#include "types.hpp"

void evaluateSSIM(const char *img1Path, const char *img2Path);
void renderFBO(std::string filename, std::vector<GLfloat> &glPatches, int patchShaderId, AABB aabb);
void setAABBProjMat(int shaderId, AABB aabb);