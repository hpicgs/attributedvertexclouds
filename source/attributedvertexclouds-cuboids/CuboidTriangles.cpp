
#include "CuboidTriangles.h"

#include <algorithm>

#include <glbinding/gl/gl.h>

#include "common.h"

using namespace gl;

CuboidTriangles::CuboidTriangles()
: m_vertices(0)
, m_vao(0)
, m_vertexShader(0)
, m_fragmentShader(0)
{
}

CuboidTriangles::~CuboidTriangles()
{
    glDeleteBuffers(1, &m_vertices);
    glDeleteVertexArrays(1, &m_vao);
}

bool CuboidTriangles::initialized() const
{
    return m_vertices > 0;
}

void CuboidTriangles::initialize()
{
    glGenBuffers(1, &m_vertices);
    glGenVertexArrays(1, &m_vao);

    initializeVAO();

    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    m_programs.resize(2);
    m_programs[0] = glCreateProgram();
    m_programs[1] = glCreateProgram();

    glAttachShader(m_programs[0], m_vertexShader);
    glAttachShader(m_programs[0], m_fragmentShader);

    glAttachShader(m_programs[1], m_vertexShader);

    loadShader();
}

void CuboidTriangles::initializeVAO()
{
    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vertices);
    glBufferData(GL_ARRAY_BUFFER, byteSize(), nullptr, GL_STATIC_DRAW);

    glBufferSubData(GL_ARRAY_BUFFER, verticesCount() * sizeof(float) * 0, verticesCount() * sizeof(float) * 3, m_vertex.data());
    glBufferSubData(GL_ARRAY_BUFFER, verticesCount() * sizeof(float) * 3, verticesCount() * sizeof(float) * 3, m_normal.data());
    glBufferSubData(GL_ARRAY_BUFFER, verticesCount() * sizeof(float) * 6, verticesCount() * sizeof(float) * 1, m_colorValue.data());
    glBufferSubData(GL_ARRAY_BUFFER, verticesCount() * sizeof(float) * 7, verticesCount() * sizeof(float) * 1, m_gradientIndex.data());

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), reinterpret_cast<void*>(verticesCount() * sizeof(float) * 0));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), reinterpret_cast<void*>(verticesCount() * sizeof(float) * 3));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), reinterpret_cast<void*>(verticesCount() * sizeof(float) * 6));
    glVertexAttribIPointer(3, 1, GL_INT, sizeof(int), reinterpret_cast<void*>(verticesCount() * sizeof(float) * 7));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool CuboidTriangles::loadShader()
{
    const auto vertexShaderSource = textFromFile("data/shaders/cuboids-triangles/standard.vert");
    const auto vertexShaderSource_ptr = vertexShaderSource.c_str();
    if(vertexShaderSource_ptr)
        glShaderSource(m_vertexShader, 1, &vertexShaderSource_ptr, 0);

    glCompileShader(m_vertexShader);

    bool success = checkForCompilationError(m_vertexShader, "vertex shader");

    const auto fragmentShaderSource = textFromFile("data/shaders/visualization.frag");
    const auto fragmentShaderSource_ptr = fragmentShaderSource.c_str();
    if(fragmentShaderSource_ptr)
        glShaderSource(m_fragmentShader, 1, &fragmentShaderSource_ptr, 0);

    glCompileShader(m_fragmentShader);

    success &= checkForCompilationError(m_fragmentShader, "fragment shader");


    if (!success)
    {
        return false;
    }

    glLinkProgram(m_programs[0]);

    success &= checkForLinkerError(m_programs[0], "program");

    glLinkProgram(m_programs[1]);

    success &= checkForLinkerError(m_programs[1], "depth only program");

    if (!success)
    {
        return false;
    }

    glBindFragDataLocation(m_programs[0], 0, "out_color");

    return true;
}

void CuboidTriangles::setCube(size_t index, const Cuboid & cuboid)
{
    static const glm::vec3 NEGATIVE_X = glm::vec3(-1.0, 0.0, 0.0);
    //static const glm::vec3 NEGATIVE_Y = glm::vec3(0.0, -1.0, 0.0);
    static const glm::vec3 NEGATIVE_Z = glm::vec3(0.0, 0.0, -1.0);
    static const glm::vec3 POSITIVE_X = glm::vec3(1.0, 0.0, 0.0);
    static const glm::vec3 POSITIVE_Y = glm::vec3(0.0, 1.0, 0.0);
    static const glm::vec3 POSITIVE_Z = glm::vec3(0.0, 0.0, 1.0);

    static const glm::vec3 vertices[8] = {
        glm::vec3(-0.5f, 0.5f, -0.5f), // A = H
        glm::vec3(-0.5f, 0.5f, 0.5f), // B = F
        glm::vec3(0.5f, 0.5f, -0.5f), // C = J
        glm::vec3(0.5f, 0.5f, 0.5f), // D
        glm::vec3(0.5f, -0.5f, 0.5f), // E = L
        glm::vec3(-0.5f, -0.5f, 0.5f), // G
        glm::vec3(-0.5f, -0.5f, -0.5f), // I
        glm::vec3(0.5f, -0.5f, -0.5f), // K
    };

    m_normal[verticesPerCuboid() * index + 0] = NEGATIVE_X;
    m_normal[verticesPerCuboid() * index + 1] = NEGATIVE_X;
    m_normal[verticesPerCuboid() * index + 2] = NEGATIVE_X;
    m_normal[verticesPerCuboid() * index + 3] = NEGATIVE_X;
    m_normal[verticesPerCuboid() * index + 4] = NEGATIVE_X;
    m_normal[verticesPerCuboid() * index + 5] = NEGATIVE_X;

    m_normal[verticesPerCuboid() * index + 6] = NEGATIVE_Z;
    m_normal[verticesPerCuboid() * index + 7] = NEGATIVE_Z;
    m_normal[verticesPerCuboid() * index + 8] = NEGATIVE_Z;
    m_normal[verticesPerCuboid() * index + 9] = NEGATIVE_Z;
    m_normal[verticesPerCuboid() * index + 10] = NEGATIVE_Z;
    m_normal[verticesPerCuboid() * index + 11] = NEGATIVE_Z;

    m_normal[verticesPerCuboid() * index + 12] = POSITIVE_X;
    m_normal[verticesPerCuboid() * index + 13] = POSITIVE_X;
    m_normal[verticesPerCuboid() * index + 14] = POSITIVE_X;
    m_normal[verticesPerCuboid() * index + 15] = POSITIVE_X;
    m_normal[verticesPerCuboid() * index + 16] = POSITIVE_X;
    m_normal[verticesPerCuboid() * index + 17] = POSITIVE_X;

    m_normal[verticesPerCuboid() * index + 18] = POSITIVE_Z;
    m_normal[verticesPerCuboid() * index + 19] = POSITIVE_Z;
    m_normal[verticesPerCuboid() * index + 20] = POSITIVE_Z;
    m_normal[verticesPerCuboid() * index + 21] = POSITIVE_Z;
    m_normal[verticesPerCuboid() * index + 22] = POSITIVE_Z;
    m_normal[verticesPerCuboid() * index + 23] = POSITIVE_Z;

    m_normal[verticesPerCuboid() * index + 24] = POSITIVE_Y;
    m_normal[verticesPerCuboid() * index + 25] = POSITIVE_Y;
    m_normal[verticesPerCuboid() * index + 26] = POSITIVE_Y;
    m_normal[verticesPerCuboid() * index + 27] = POSITIVE_Y;
    m_normal[verticesPerCuboid() * index + 28] = POSITIVE_Y;
    m_normal[verticesPerCuboid() * index + 29] = POSITIVE_Y;

    m_vertex[verticesPerCuboid() * index + 0] = vertices[1];
    m_vertex[verticesPerCuboid() * index + 1] = vertices[0];
    m_vertex[verticesPerCuboid() * index + 2] = vertices[5];
    m_vertex[verticesPerCuboid() * index + 3] = vertices[5];
    m_vertex[verticesPerCuboid() * index + 4] = vertices[0];
    m_vertex[verticesPerCuboid() * index + 5] = vertices[6];

    m_vertex[verticesPerCuboid() * index + 6] = vertices[6];
    m_vertex[verticesPerCuboid() * index + 7] = vertices[0];
    m_vertex[verticesPerCuboid() * index + 8] = vertices[7];
    m_vertex[verticesPerCuboid() * index + 9] = vertices[7];
    m_vertex[verticesPerCuboid() * index + 10] = vertices[0];
    m_vertex[verticesPerCuboid() * index + 11] = vertices[2];

    m_vertex[verticesPerCuboid() * index + 12] = vertices[2];
    m_vertex[verticesPerCuboid() * index + 13] = vertices[3];
    m_vertex[verticesPerCuboid() * index + 14] = vertices[7];
    m_vertex[verticesPerCuboid() * index + 15] = vertices[7];
    m_vertex[verticesPerCuboid() * index + 16] = vertices[3];
    m_vertex[verticesPerCuboid() * index + 17] = vertices[4];

    m_vertex[verticesPerCuboid() * index + 18] = vertices[4];
    m_vertex[verticesPerCuboid() * index + 19] = vertices[3];
    m_vertex[verticesPerCuboid() * index + 20] = vertices[5];
    m_vertex[verticesPerCuboid() * index + 21] = vertices[5];
    m_vertex[verticesPerCuboid() * index + 22] = vertices[3];
    m_vertex[verticesPerCuboid() * index + 23] = vertices[1];

    m_vertex[verticesPerCuboid() * index + 24] = vertices[1];
    m_vertex[verticesPerCuboid() * index + 25] = vertices[3];
    m_vertex[verticesPerCuboid() * index + 26] = vertices[0];
    m_vertex[verticesPerCuboid() * index + 27] = vertices[0];
    m_vertex[verticesPerCuboid() * index + 28] = vertices[3];
    m_vertex[verticesPerCuboid() * index + 29] = vertices[2];

    for (int i = 0; i < verticesPerCuboid(); ++i)
    {
        m_vertex[verticesPerCuboid() * index + i] = cuboid.center + cuboid.extent * m_vertex[verticesPerCuboid() * index + i];
        m_colorValue[verticesPerCuboid() * index + i] = cuboid.colorValue;
        m_gradientIndex[verticesPerCuboid() * index + i] = cuboid.gradientIndex;
    }
}

size_t CuboidTriangles::size() const
{
    return m_multiStarts.size();
}

size_t CuboidTriangles::verticesPerCuboid() const
{
    return 36;
}

size_t CuboidTriangles::verticesCount() const
{
    return size() * verticesPerCuboid();
}

size_t CuboidTriangles::byteSize() const
{
    return verticesCount() * vertexByteSize();
}

size_t CuboidTriangles::vertexByteSize() const
{
    return sizeof(float) * componentCount();
}

size_t CuboidTriangles::componentCount() const
{
    return 8;
}

void CuboidTriangles::reserve(size_t count)
{
    m_vertex.reserve(count * verticesPerCuboid());
    m_normal.reserve(count * verticesPerCuboid());
    m_colorValue.reserve(count * verticesPerCuboid());
    m_gradientIndex.reserve(count * verticesPerCuboid());

    m_multiStarts.resize(count);
    m_multiCounts.resize(count);

    size_t next = 0;
    std::fill(m_multiCounts.begin(), m_multiCounts.end(), verticesPerCuboid());
    std::generate(m_multiStarts.begin(), m_multiStarts.end(), [this, &next]() {
        auto current = next;
        next += verticesPerCuboid();
        return current;
    });
}

void CuboidTriangles::resize(size_t count)
{
    m_vertex.resize(count * verticesPerCuboid());
    m_normal.resize(count * verticesPerCuboid());
    m_colorValue.resize(count * verticesPerCuboid());
    m_gradientIndex.resize(count * verticesPerCuboid());

    m_multiStarts.resize(count);
    m_multiCounts.resize(count);

    size_t next = 0;
    std::fill(m_multiCounts.begin(), m_multiCounts.end(), verticesPerCuboid());
    std::generate(m_multiStarts.begin(), m_multiStarts.end(), [this, &next]() {
        auto current = next;
        next += verticesPerCuboid();
        return current;
    });
}

void CuboidTriangles::render()
{
    glBindVertexArray(m_vao);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glDisable(GL_CULL_FACE);
    //glCullFace(GL_BACK);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    // Pre-Z Pass
    //glDepthFunc(GL_LEQUAL);
    //glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    //glUseProgram(m_programs[1]);
    //glMultiDrawArrays(GL_POINTS, m_multiStarts.data(), m_multiCounts.data(), size());

    // Color Pass
    //glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(m_programs[0]);
    glMultiDrawArrays(GL_TRIANGLES, m_multiStarts.data(), m_multiCounts.data(), size());

    glUseProgram(0);

    glBindVertexArray(0);
}

const std::vector<gl::GLuint> & CuboidTriangles::programs() const
{
    return m_programs;
}
