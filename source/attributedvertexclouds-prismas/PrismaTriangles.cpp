
#include "PrismaTriangles.h"

#include <glbinding/gl/gl.h>

#include "common.h"

using namespace gl;

PrismaTriangles::PrismaTriangles()
: PrismaImplementation("Triangles")
, m_vertices(0)
, m_vao(0)
, m_vertexShader(0)
, m_fragmentShader(0)
{
}

PrismaTriangles::~PrismaTriangles()
{
    glDeleteBuffers(1, &m_vertices);
    glDeleteVertexArrays(1, &m_vao);

    glDeleteShader(m_vertexShader);
    glDeleteShader(m_fragmentShader);
    glDeleteProgram(m_program);
}

void PrismaTriangles::onInitialize()
{
    glGenBuffers(1, &m_vertices);

    glGenVertexArrays(1, &m_vao);

    initializeVAO();

    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    m_program = glCreateProgram();

    glAttachShader(m_program, m_vertexShader);
    glAttachShader(m_program, m_fragmentShader);

    loadShader();
}

void PrismaTriangles::initializeVAO()
{
    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vertices);
    glBufferData(GL_ARRAY_BUFFER, size() * vertexByteSize(), nullptr, GL_STATIC_DRAW);

    glBufferSubData(GL_ARRAY_BUFFER, size() * sizeof(float) * 0, size() * sizeof(float) * 3, m_position.data());
    glBufferSubData(GL_ARRAY_BUFFER, size() * sizeof(float) * 3, size() * sizeof(float) * 3, m_normal.data());
    glBufferSubData(GL_ARRAY_BUFFER, size() * sizeof(float) * 6, size() * sizeof(float) * 1, m_colorValue.data());

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), reinterpret_cast<void*>(size() * sizeof(float) * 0));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), reinterpret_cast<void*>(size() * sizeof(float) * 3));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), reinterpret_cast<void*>(size() * sizeof(float) * 6));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool PrismaTriangles::loadShader()
{
    const auto vertexShaderSource = textFromFile("data/shaders/visualization-triangles/standard.vert");
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

    glLinkProgram(m_program);

    success &= checkForLinkerError(m_program, "program");

    if (!success)
    {
        return false;
    }

    glBindFragDataLocation(m_program, 0, "out_color");

    return true;
}

void PrismaTriangles::setPrisma(size_t index, const Prisma & prisma)
{
    if (prisma.points.size() < 3)
    {
        return;
    }

    const auto vertexCount = (4 * prisma.points.size() - 4) * 3;

    m_mutex.lock();

    const auto firstIndex = m_position.size();
    const auto topFaceStartIndex = firstIndex + 2 * prisma.points.size() * 3;
    const auto bottomFaceStartIndex = topFaceStartIndex + (prisma.points.size() - 2) * 3;

    m_position.resize(m_position.size() + vertexCount);
    m_normal.resize(m_normal.size() + vertexCount);
    m_colorValue.resize(m_colorValue.size() + vertexCount);

    for (auto i = size_t(0); i < prisma.points.size(); ++i)
    {
        if (i >= 2)
        {
            // Top face
            m_position[topFaceStartIndex + 3*(i-2)+0] = glm::vec3(prisma.points[i-1].x, prisma.heightRange.y, prisma.points[i-1].y);
            m_position[topFaceStartIndex + 3*(i-2)+1] = glm::vec3(prisma.points[0].x, prisma.heightRange.y, prisma.points[0].y);
            m_position[topFaceStartIndex + 3*(i-2)+2] = glm::vec3(prisma.points[i].x, prisma.heightRange.y, prisma.points[i].y);
            m_normal[topFaceStartIndex + 3*(i-2)+0] = glm::vec3(0.0f, 1.0f, 0.0f);
            m_normal[topFaceStartIndex + 3*(i-2)+1] = glm::vec3(0.0f, 1.0f, 0.0f);
            m_normal[topFaceStartIndex + 3*(i-2)+2] = glm::vec3(0.0f, 1.0f, 0.0f);
            m_colorValue[topFaceStartIndex + 3*(i-2)+0] = prisma.colorValue;
            m_colorValue[topFaceStartIndex + 3*(i-2)+1] = prisma.colorValue;
            m_colorValue[topFaceStartIndex + 3*(i-2)+2] = prisma.colorValue;

            // Bottom face
            m_position[bottomFaceStartIndex + 3*(i-2)+0] = glm::vec3(prisma.points[i].x, prisma.heightRange.x, prisma.points[i].y);
            m_position[bottomFaceStartIndex + 3*(i-2)+1] = glm::vec3(prisma.points[0].x, prisma.heightRange.x, prisma.points[0].y);
            m_position[bottomFaceStartIndex + 3*(i-2)+2] = glm::vec3(prisma.points[i-1].x, prisma.heightRange.x, prisma.points[i-1].y);
            m_normal[bottomFaceStartIndex + 3*(i-2)+0] = glm::vec3(0.0f, -1.0f, 0.0f);
            m_normal[bottomFaceStartIndex + 3*(i-2)+1] = glm::vec3(0.0f, -1.0f, 0.0f);
            m_normal[bottomFaceStartIndex + 3*(i-2)+2] = glm::vec3(0.0f, -1.0f, 0.0f);
            m_colorValue[bottomFaceStartIndex + 3*(i-2)+0] = prisma.colorValue;
            m_colorValue[bottomFaceStartIndex + 3*(i-2)+1] = prisma.colorValue;
            m_colorValue[bottomFaceStartIndex + 3*(i-2)+2] = prisma.colorValue;
        }

        // Side face
        const auto & current = prisma.points[i];
        const auto & next = prisma.points[(i+1) % prisma.points.size()];

        const auto normal = glm::cross(glm::vec3(next.x - current.x, 0.0f, next.y - current.y), glm::vec3(0.0f, 1.0f, 0.0f));

        m_position[firstIndex + 6*i+0] = glm::vec3(next.x, prisma.heightRange.x, next.y);
        m_position[firstIndex + 6*i+1] = glm::vec3(current.x, prisma.heightRange.x, current.y);
        m_position[firstIndex + 6*i+2] = glm::vec3(next.x, prisma.heightRange.y, next.y);
        m_position[firstIndex + 6*i+3] = glm::vec3(next.x, prisma.heightRange.y, next.y);
        m_position[firstIndex + 6*i+4] = glm::vec3(current.x, prisma.heightRange.x, current.y);
        m_position[firstIndex + 6*i+5] = glm::vec3(current.x, prisma.heightRange.y, current.y);
        m_normal[firstIndex + 6*i+0] = normal;
        m_normal[firstIndex + 6*i+1] = normal;
        m_normal[firstIndex + 6*i+2] = normal;
        m_normal[firstIndex + 6*i+3] = normal;
        m_normal[firstIndex + 6*i+4] = normal;
        m_normal[firstIndex + 6*i+5] = normal;
        m_colorValue[firstIndex + 6*i+0] = prisma.colorValue;
        m_colorValue[firstIndex + 6*i+1] = prisma.colorValue;
        m_colorValue[firstIndex + 6*i+2] = prisma.colorValue;
        m_colorValue[firstIndex + 6*i+3] = prisma.colorValue;
        m_colorValue[firstIndex + 6*i+4] = prisma.colorValue;
        m_colorValue[firstIndex + 6*i+5] = prisma.colorValue;
    }

    m_mutex.unlock();
}

size_t PrismaTriangles::size() const
{
    return m_position.size();
}

size_t PrismaTriangles::verticesCount() const
{
    return size();
}

size_t PrismaTriangles::staticByteSize() const
{
    return 0;
}

size_t PrismaTriangles::byteSize() const
{
    return size() * vertexByteSize();
}

size_t PrismaTriangles::vertexByteSize() const
{
    return sizeof(float) * componentCount();
}

size_t PrismaTriangles::componentCount() const
{
    return 7;
}

void PrismaTriangles::resize(size_t count)
{
}

void PrismaTriangles::onRender()
{
    glBindVertexArray(m_vao);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    glUseProgram(m_program);
    glDrawArrays(GL_TRIANGLES, 0, verticesCount());

    glUseProgram(0);

    glBindVertexArray(0);
}

gl::GLuint PrismaTriangles::program() const
{
    return m_program;
}