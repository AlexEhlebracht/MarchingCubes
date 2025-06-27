#include "../include/WaterMesh.h"
#include <vector>

WaterMesh::WaterMesh(float yLevel, float size)
{
    float half = size * 0.5f;
    std::vector<glm::vec3> verts = {
        {-half, yLevel, -half},
        { half, yLevel, -half},
        { half, yLevel,  half},
        {-half, yLevel,  half}
    };

    std::vector<glm::vec3> norms(4, { 0, 1, 0 });
    std::vector<glm::vec3> cols(4, { 0, 0, 1 }); // unused in shader
    std::vector<unsigned> idx = { 0, 1, 2, 0, 2, 3 };

    mesh = new Mesh(verts, cols, norms, idx);
}

void WaterMesh::draw(const Shader& shader)
{
    mesh->draw();
}