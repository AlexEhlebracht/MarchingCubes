#pragma once
#include "Mesh.h"

class WaterMesh
{
public:
    WaterMesh(float yLevel, float size);
    void draw(const Shader& shader);
private:
    Mesh* mesh;
};
