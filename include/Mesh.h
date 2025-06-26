#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "Shader.h"

class Mesh
{
public:
    Mesh(const std::vector<glm::vec3>& vertices,
        const std::vector<glm::vec3>& colors,
        const std::vector<glm::vec3>& normals,
        const std::vector<unsigned int>& indices);

    ~Mesh();

    void draw() const;

private:
    unsigned int VAO, VBO_Vertices, VBO_Colors, VBO_Normals, EBO;
    unsigned int indexCount;

    void setupMesh(const std::vector<glm::vec3>& vertices,
        const std::vector<glm::vec3>& colors,
        const std::vector<glm::vec3>& normals,
        const std::vector<unsigned int>& indices);
};
