
#include "Rendering.h"

#include <iostream>
#include <algorithm>

#include <glm/gtc/type_ptr.hpp>

#include <glbinding/gl/gl.h>

#include "common.h"

#include "Implementation.h"
#include "Postprocessing.h"


using namespace gl;


namespace
{


static const auto fpsSampleCount = size_t(100);


} // namespace


Rendering::Rendering()
: m_current(nullptr)
, m_postprocessing(nullptr)
, m_query(0)
, m_width(0)
, m_height(0)
, m_gridSize(32)
, m_usePostprocessing(false)
, m_rasterizerDiscard(false)
, m_fpsSamples(fpsSampleCount+1)
{
}

Rendering::~Rendering()
{
    // Flag all aquired resources for deletion (hint: driver decides when to actually delete them; see: shared contexts)
    glDeleteQueries(1, &m_query);

    delete m_postprocessing;

    for (auto implementation : m_implementations)
    {
        delete implementation;
    }
}

void Rendering::addImplementation(Implementation *implementation)
{
    m_implementations.push_back(implementation);
}

void Rendering::initialize()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);

    glGenQueries(1, &m_query);

    m_postprocessing = new Postprocessing;

    onInitialize();
    onCreateGeometry();

    m_start = std::chrono::high_resolution_clock::now();

    setTechnique(0);
}

void Rendering::reloadShaders()
{
    for (auto implementation : m_implementations)
    {
        if (implementation->initialized())
        {
            implementation->loadShader();
        }
    }

    m_postprocessing->loadShader();
}

void Rendering::cameraPosition(glm::vec3 & eye, glm::vec3 & center, glm::vec3 & up) const
{
    static const auto eye0 = glm::vec3(1.1f, 1.1f, 1.1f);
    static const auto eye1 = glm::vec3(1.2f, 0.0f, 1.2f);
    static const auto eye2 = glm::vec3(1.4f, 0.0f, 0.0f);

    static const auto center0 = glm::vec3(0.0f, 0.0f, 0.0f);
    static const auto up0 = glm::vec3(0.0f, 1.0f, 0.0f);

    const auto f = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_start).count()) / 1000.0f;

    switch (m_cameraSetting)
    {
    case 0:
        eye = cameraPath(eye0, f);
        center = center0;
        up = up0;
        break;
    case 1:
        eye = eye0;
        center = center0;
        up = up0;
        break;
    case 2:
        eye = eye1;
        center = center0;
        up = up0;
        break;
    case 3:
        eye = eye2;
        center = center0;
        up = up0;
        break;
    default:
        eye = cameraPath(eye0, f);
        center = center0;
        up = up0;
        break;
    }
}

void Rendering::prepareRendering()
{
    auto eye = glm::vec3(0.0f, 0.0f, 0.0f);
    auto center = glm::vec3(0.0f, 0.0f, 0.0f);
    auto up = glm::vec3(0.0f, 0.0f, 0.0f);

    cameraPosition(eye, center, up);

    const auto view = glm::lookAt(eye, center, up);
    const auto viewProjection = glm::perspectiveFov(glm::radians(45.0f), float(m_width), float(m_height), 0.05f, 2.5f) * view;

    GLuint program = m_current->program();
    const auto viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
    const auto gradientSamplerLocation = glGetUniformLocation(program, "gradient");
    glUseProgram(program);
    glUniformMatrix4fv(viewProjectionLocation, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glUniform1i(gradientSamplerLocation, 0);

    glUseProgram(0);

    onPrepareRendering();
}

void Rendering::finalizeRendering()
{
    onFinalizeRendering();
}

void Rendering::resize(int w, int h)
{
    m_width = w;
    m_height = h;

    if (m_postprocessing && m_postprocessing->initialized())
    {
        m_postprocessing->resize(m_width, m_height);
    }
}

void Rendering::setCameraTechnique(int i)
{
    if (i < 0 || i >= 4)
    {
        return;
    }

    m_cameraSetting = i;
}

void Rendering::setTechnique(int i)
{
    if (i < 0 || i >= m_implementations.size())
    {
        return;
    }

    m_current = m_implementations.at(i);

    std::cout << "Switch to " << m_current->name() << " implementation" << std::endl;
}

void Rendering::render()
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

    m_current->initialize();

    glViewport(0, 0, m_width, m_height);

    if (m_usePostprocessing && !m_rasterizerDiscard)
    {
        static const float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };

        if (!m_postprocessing->initialized())
        {
            m_postprocessing->initialize();
            m_postprocessing->resize(m_width, m_height);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, m_postprocessing->fbo());

        glClearBufferfv(GL_COLOR, 0, white);
        glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    prepareRendering();

    if (m_rasterizerDiscard)
    {
        glEnable(GL_RASTERIZER_DISCARD);
    }

    m_current->render();

    if (m_rasterizerDiscard)
    {
        glDisable(GL_RASTERIZER_DISCARD);
    }

    finalizeRendering();

    if (m_usePostprocessing && !m_rasterizerDiscard)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_postprocessing->render();
    }
}

void Rendering::spaceMeasurement()
{
    const auto reference = std::accumulate(m_implementations.begin(), m_implementations.end(),
            std::accumulate(m_implementations.begin(), m_implementations.end(), 0, [](size_t currentSize, const Implementation * technique) {
                return std::max(currentSize, technique->fullByteSize());
            }), [](size_t currentSize, const Implementation * technique) {
        return std::min(currentSize, technique->fullByteSize());
    });

    const auto printSpaceMeasurement = [&reference](const std::string & techniqueName, size_t byteSize)
    {
        std::cout << techniqueName << std::endl << (byteSize / 1024) << "kB (" << (static_cast<float>(byteSize) / reference) << "x)" << std::endl;
    };

    std::cout << "Count: " << primitiveCount() << std::endl;
    std::cout << std::endl;

    for (const auto implementation : m_implementations)
    {
        printSpaceMeasurement(implementation->name(), implementation->fullByteSize());
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

void Rendering::toggleRasterizerDiscard()
{
    m_rasterizerDiscard = !m_rasterizerDiscard;
}

void Rendering::startFPSMeasuring()
{
    m_fpsSamples = 0;
    m_fpsMeasurementStart = std::chrono::high_resolution_clock::now();
}

void Rendering::togglePostprocessing()
{
    m_usePostprocessing = !m_usePostprocessing;
}

void Rendering::setGridSize(int gridSize)
{
    m_gridSize = gridSize;
}
