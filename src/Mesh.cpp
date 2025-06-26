#include "../include/Mesh.h"
#include <glad/glad.h>

Mesh::Mesh(const std::vector<glm::vec3>& vertices,
    const std::vector<glm::vec3>& colors,
    const std::vector<glm::vec3>& normals,
    const std::vector<unsigned int>& indices)
{
    indexCount = (unsigned int)indices.size();
    setupMesh(vertices, colors, normals, indices);
}

Mesh::~Mesh()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO_Vertices);
    glDeleteBuffers(1, &VBO_Colors);
    glDeleteBuffers(1, &VBO_Normals);
    glDeleteBuffers(1, &EBO);
}

void Mesh::setupMesh(const std::vector<glm::vec3>& vertices,
    const std::vector<glm::vec3>& colors,
    const std::vector<glm::vec3>& normals,
    const std::vector<unsigned int>& indices)
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO_Vertices);
    glGenBuffers(1, &VBO_Colors);
    glGenBuffers(1, &VBO_Normals);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Vertex positions
    glBindBuffer(GL_ARRAY_BUFFER, VBO_Vertices);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    // Colors
    glBindBuffer(GL_ARRAY_BUFFER, VBO_Colors);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec3), colors.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    // Normals
    glBindBuffer(GL_ARRAY_BUFFER, VBO_Normals);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), normals.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void Mesh::draw() const
{
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
