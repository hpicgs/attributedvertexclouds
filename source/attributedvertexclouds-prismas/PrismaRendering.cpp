
#include "PrismaRendering.h"

#include <iostream>
#include <chrono>
#include <algorithm>

#include <glm/gtc/random.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glbinding/gl/gl.h>

#include "common.h"

#include "PrismaVertexCloud.h"
//#include "PrismaTriangles.h"
//#include "PrismaTriangleStrip.h"
//#include "PrismaInstancing.h"


using namespace gl;


namespace
{


static const auto prismaGridSize = size_t(48);
static const auto prismaCount = prismaGridSize * prismaGridSize * prismaGridSize;
static const auto fpsSampleCount = size_t(100);

static const auto worldScale = glm::vec3(1.0f) / glm::vec3(prismaGridSize, prismaGridSize, prismaGridSize);
static const auto gridOffset = 0.2f;

static const auto lightGray = glm::vec3(234) / 275.0f;
static const auto red = glm::vec3(196, 30, 20) / 275.0f;
static const auto orange = glm::vec3(255, 114, 70) / 275.0f;
static const auto yellow = glm::vec3(255, 200, 107) / 275.0f;


} // namespace


PrismaRendering::PrismaRendering()
: m_current(nullptr)
, m_query(0)
, m_gradientTexture(0)
, m_rasterizerDiscard(false)
, m_fpsSamples(fpsSampleCount+1)
{
    m_implementations[0] = new PrismaVertexCloud;//new PrismaTriangles;
    m_implementations[1] = new PrismaVertexCloud;//new PrismaTriangleStrip;
    m_implementations[2] = new PrismaVertexCloud;//new PrismaInstancing;
    m_implementations[3] = new PrismaVertexCloud;

    setTechnique(0);
}

PrismaRendering::~PrismaRendering()
{
    // Flag all aquired resources for deletion (hint: driver decides when to actually delete them; see: shared contexts)
    glDeleteQueries(1, &m_query);
    glDeleteTextures(1, &m_gradientTexture);

    delete m_implementations[0];
    delete m_implementations[1];
    delete m_implementations[2];
    delete m_implementations[3];
}

void PrismaRendering::initialize()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);

    createGeometry();

    glGenQueries(1, &m_query);

    glGenTextures(1, &m_gradientTexture);

    std::array<glm::vec3, 4> gradient = {{
        red,
        orange,
        yellow,
        lightGray
    }};

    glBindTexture(GL_TEXTURE_1D, m_gradientTexture);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB32F, gradient.size(), 0, GL_RGB, GL_FLOAT, gradient.data());
    glBindTexture(GL_TEXTURE_1D, 0);

    m_start = std::chrono::high_resolution_clock::now();
}

void PrismaRendering::reloadShaders()
{
    for (auto implementation : m_implementations)
    {
        if (implementation->initialized())
        {
            implementation->loadShader();
        }
    }
}

void PrismaRendering::createGeometry()
{
    for (auto implementation : m_implementations)
    {
        implementation->resize(prismaCount);
    }

    std::array<std::vector<float>, 4> noise;
    for (auto i = size_t(0); i < noise.size(); ++i)
    {
        noise[i] = rawFromFileF("data/noise/noise-48-"+std::to_string(i)+".raw");
    }

#pragma omp parallel for
    for (size_t i = 0; i < prismaCount; ++i)
    {
        const auto position = glm::ivec3(i % prismaGridSize, (i / prismaGridSize) % prismaGridSize, i / prismaGridSize / prismaGridSize);
        const auto offset = glm::vec3(
            (position.y + position.z) % 2 ? gridOffset : 0.0f,
            (position.x + position.z) % 2 ? gridOffset : 0.0f,
            (position.x + position.y) % 2 ? gridOffset : 0.0f
        );

        Prisma p;

        p.heightRange.x = -0.5f + (position.y + offset.y) * worldScale.y - 0.5f * noise[0][i] * worldScale.y;
        p.heightRange.y = -0.5f + (position.y + offset.y) * worldScale.y + 0.5f * noise[0][i] * worldScale.y;

        const auto vertexCount = size_t(3) + size_t(glm::ceil(12.0f * noise[1][i]));
        const auto center = glm::vec2(-0.5f, -0.5f) + (glm::vec2(position.x, position.z) + glm::vec2(offset.x, offset.z)) * glm::vec2(worldScale.x, worldScale.z);
        const auto radius = 0.5f * 0.5f * (noise[2][i] + 1.0f);

        p.points.resize(vertexCount);

        for (auto j = size_t(0); j < vertexCount; ++j)
        {
            const auto angle = glm::pi<float>() * 2.0f * float(j) / float(vertexCount);
            const auto normalizedPosition = glm::vec2(
                glm::cos(angle),
                glm::sin(angle)
            );

            p.points[j] = center + glm::vec2(radius, radius) * normalizedPosition * glm::vec2(worldScale.x, worldScale.z);
        }

        p.colorValue = noise[3][i];

        for (auto implementation : m_implementations)
        {
            implementation->setPrisma(i, p);
        }
    }
}

void PrismaRendering::updateUniforms()
{
    static const auto eye = glm::vec3(1.0f, 1.0f, 1.0f);
    static const auto center = glm::vec3(0.0f, 0.0f, 0.0f);
    static const auto up = glm::vec3(0.0f, 1.0f, 0.0f);

    const auto f = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_start).count()) / 1000.0f;

    const auto view = glm::lookAt(cameraPath(eye, f), center, up);
    const auto viewProjection = glm::perspectiveFov(glm::radians(45.0f), float(m_width), float(m_height), 0.05f, 2.0f) * view;

    GLuint program = m_current->program();
    const auto viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
    const auto gradientSamplerLocation = glGetUniformLocation(program, "gradient");
    glUseProgram(program);
    glUniformMatrix4fv(viewProjectionLocation, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glUniform1i(gradientSamplerLocation, 0);

    glUseProgram(0);
}

void PrismaRendering::resize(int w, int h)
{
    m_width = w;
    m_height = h;
}

void PrismaRendering::setTechnique(int i)
{
    m_current = m_implementations.at(i);

    switch (i)
    {
    case 0:
        std::cout << "Switch to Triangles implementation" << std::endl;
        break;
    case 1:
        std::cout << "Switch to TriangleStrip implementation" << std::endl;
        break;
    case 2:
        std::cout << "Switch to Instancing implementation" << std::endl;
        break;
    case 3:
        std::cout << "Switch to AttributedVertexCloud implementation" << std::endl;
        break;
    }
}

void PrismaRendering::render()
{
    if (m_fpsSamples == fpsSampleCount)
    {
        const auto end = std::chrono::high_resolution_clock::now();

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_fpsMeasurementStart).count() / 1000.0f / fpsSampleCount;

        std::cout << "Measured " << (1.0f / elapsed) << "FPS (" << "(~ " << (elapsed * 1000.0f) << "ms per frame)" << std::endl;

        m_fpsSamples = fpsSampleCount + 1;
    }

    if (m_fpsSamples < fpsSampleCount)
    {
        ++m_fpsSamples;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, m_width, m_height);

    m_current->initialize();

    updateUniforms();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, m_gradientTexture);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_rasterizerDiscard)
    {
        glEnable(GL_RASTERIZER_DISCARD);
    }

    m_current->render();

    if (m_rasterizerDiscard)
    {
        glDisable(GL_RASTERIZER_DISCARD);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, 0);
}

void PrismaRendering::spaceMeasurement()
{
    const auto reference = std::accumulate(m_implementations.begin(), m_implementations.end(),
            std::accumulate(m_implementations.begin(), m_implementations.end(), 0, [](size_t currentSize, const PrismaImplementation * technique) {
                return std::max(currentSize, technique->fullByteSize());
            }), [](size_t currentSize, const PrismaImplementation * technique) {
        return std::min(currentSize, technique->fullByteSize());
    });

    const auto printSpaceMeasurement = [&reference](const std::string & techniqueName, size_t byteSize)
    {
        std::cout << techniqueName << std::endl << (byteSize / 1024) << "kB (" << (static_cast<float>(byteSize) / reference) << "x)" << std::endl;
    };

    std::cout << "Prisma count: " << prismaCount << std::endl;
    std::cout << std::endl;

    for (const auto implementation : m_implementations)
    {
        printSpaceMeasurement(implementation->name(), implementation->fullByteSize());
    }
}

void PrismaRendering::measureCPU(const std::string & name, std::function<void()> callback, bool on) const
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

void PrismaRendering::measureGPU(const std::string & name, std::function<void()> callback, bool on) const
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

void PrismaRendering::toggleRasterizerDiscard()
{
    m_rasterizerDiscard = !m_rasterizerDiscard;
}

void PrismaRendering::startFPSMeasuring()
{
    m_fpsSamples = 0;
    m_fpsMeasurementStart = std::chrono::high_resolution_clock::now();
}