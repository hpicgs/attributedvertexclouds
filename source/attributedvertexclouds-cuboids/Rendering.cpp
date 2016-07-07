
#include "Rendering.h"

#include <iostream>
#include <chrono>

#include <glm/gtc/random.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glbinding/gl/gl.h>

#include "common.h"


using namespace gl;


Rendering::Rendering()
: m_current(CuboidTechnique::Triangles)
, m_query(0)
, m_measure(false)
{
}

Rendering::~Rendering()
{
    // Flag all aquired resources for deletion (hint: driver decides when to actually delete them; see: shared contexts)
    glDeleteQueries(1, &m_query);
}

void Rendering::initialize()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);

    createGeometry();

    glGenQueries(1, &m_query);

    m_start = std::chrono::high_resolution_clock::now();
}

void Rendering::reloadShaders()
{
    if (m_triangles.initialized())
    {
        m_triangles.loadShader();
    }

    if (m_triangleStrip.initialized())
    {
        m_triangleStrip.loadShader();
    }

    if (m_avc.initialized())
    {
        m_avc.loadShader();
    }
}

void Rendering::createGeometry()
{
    static const size_t cuboidCount = 100000;

    m_triangles.resize(cuboidCount);
    m_triangleStrip.resize(cuboidCount);
    m_avc.resize(cuboidCount);

#pragma omp parallel for
    for (size_t i = 0; i < cuboidCount; ++i)
    {
        Cuboid c;
        c.center = glm::vec3(glm::linearRand(-8.0f, 8.0f), glm::linearRand(-0.5f, 0.5f), glm::linearRand(-8.0f, 8.0f));
        c.extent = glm::vec3(glm::linearRand(0.1f, 0.4f), glm::linearRand(0.1f, 0.4f), glm::linearRand(0.1f, 0.4f));
        c.colorValue = glm::linearRand(0.0f, 1.0f);
        c.gradientIndex = 0;

        m_triangles.setCube(i, c);
        m_triangleStrip.setCube(i, c);
        m_avc.setCube(i, c);
    }
}

void Rendering::updateUniforms()
{
    static const auto eye = glm::vec3(1.0f, 12.0f, 1.0f);
    static const auto center = glm::vec3(0.0f, 0.0f, 0.0f);
    static const auto up = glm::vec3(0.0f, 1.0f, 0.0f);

    const auto f = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_start).count()) / 1000.0f;

    auto eyeRotation = glm::mat4(1.0f);
    eyeRotation = glm::rotate(eyeRotation, glm::sin(0.8342378f * f), glm::vec3(0.0f, 1.0f, 0.0f));
    eyeRotation = glm::rotate(eyeRotation, glm::cos(-0.5423543f * f), glm::vec3(1.0f, 0.0f, 0.0f));
    eyeRotation = glm::rotate(eyeRotation, glm::sin(0.13234823f * f), glm::vec3(0.0f, 0.0f, 1.0f));

    const auto rotatedEye = eyeRotation * glm::vec4(eye, 1.0f);
    //const auto rotatedEye = glm::vec3(12.0f, 0.0f, 0.0f);

    const auto view = glm::lookAt(glm::vec3(rotatedEye), center, up);
    const auto viewProjection = glm::perspectiveFov(glm::radians(45.0f), float(m_width), float(m_height), 1.0f, 30.0f) * view;

    if (m_triangles.initialized())
    {
        for (GLuint program : m_triangles.programs())
        {
            const auto viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
            glUseProgram(program);
            glUniformMatrix4fv(viewProjectionLocation, 1, GL_FALSE, glm::value_ptr(viewProjection));
        }
    }

    if (m_triangleStrip.initialized())
    {
        for (GLuint program : m_triangleStrip.programs())
        {
            const auto viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
            glUseProgram(program);
            glUniformMatrix4fv(viewProjectionLocation, 1, GL_FALSE, glm::value_ptr(viewProjection));
        }
    }

    if (m_avc.initialized())
    {
        for (GLuint program : m_avc.programs())
        {
            const auto viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
            glUseProgram(program);
            glUniformMatrix4fv(viewProjectionLocation, 1, GL_FALSE, glm::value_ptr(viewProjection));
        }
    }

    glUseProgram(0);
}

void Rendering::resize(int w, int h)
{
    m_width = w;
    m_height = h;
}

void Rendering::setTechnique(int i)
{
    m_current = static_cast<CuboidTechnique>(i);
}

void Rendering::render()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, m_width, m_height);

    switch (m_current)
    {
    case CuboidTechnique::Triangles:
        if (!m_triangles.initialized())
        {
            m_triangles.initialize();
        }
        break;
    case CuboidTechnique::TriangleStrip:
        if (!m_triangleStrip.initialized())
        {
            m_triangleStrip.initialize();
        }
        break;
    case CuboidTechnique::VertexCloud:
        if (!m_avc.initialized())
        {
            m_avc.initialize();
        }
        break;
    }

    updateUniforms();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    switch (m_current)
    {
    case CuboidTechnique::Triangles:
        measureGPU("rendering", [this]() {
            m_triangles.render();
        }, m_measure);
        break;
    case CuboidTechnique::TriangleStrip:
        measureGPU("rendering", [this]() {
            m_triangleStrip.render();
        }, m_measure);
        break;
    case CuboidTechnique::VertexCloud:
        measureGPU("rendering", [this]() {
            m_avc.render();
        }, m_measure);
        break;
    }
}

void Rendering::measureCPU(const std::string & name, std::function<void()> callback, bool on) const
{
    if (!on)
    {
        return callback();
    }

    const auto start = std::chrono::high_resolution_clock::now();

    callback();

    const auto end = std::chrono::high_resolution_clock::now();

    std::cout << name << ": " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << "ns" << std::endl;
}

void Rendering::measureGPU(const std::string & name, std::function<void()> callback, bool on) const
{
    if (!on)
    {
        return callback();
    }

    glBeginQuery(gl::GL_TIME_ELAPSED, m_query);

    callback();

    glEndQuery(gl::GL_TIME_ELAPSED);

    int available = 0;
    while (!available)
    {
        glGetQueryObjectiv(m_query, gl::GL_QUERY_RESULT_AVAILABLE, &available);
    }

    int value;
    glGetQueryObjectiv(m_query, gl::GL_QUERY_RESULT, &value);

    std::cout << name << ": " << value << "ns" << std::endl;
}

void Rendering::toggleMeasurements()
{
    m_measure = !m_measure;
}
