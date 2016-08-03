
#include <chrono>

#include <glbinding/gl/types.h>


class PrismaImplementation;


class PrismaRendering
{
public:
    PrismaRendering();
    ~PrismaRendering();

    void initialize();
    void createGeometry();

    void resize(int w, int h);
    void render();

    void setTechnique(int i);
    void toggleRasterizerDiscard();
    void spaceMeasurement();
    void reloadShaders();
    void startFPSMeasuring();

    void measureGPU(const std::string & name, std::function<void()> callback, bool on) const;
    void measureCPU(const std::string & name, std::function<void()> callback, bool on) const;

protected:
    PrismaImplementation * m_current;
    std::array<PrismaImplementation *, 4> m_implementations;

    gl::GLuint m_query;
    gl::GLuint m_gradientTexture;

    int m_width;
    int m_height;

    std::chrono::high_resolution_clock::time_point m_start;

    bool m_rasterizerDiscard;

    size_t m_fpsSamples;
    std::chrono::high_resolution_clock::time_point m_fpsMeasurementStart;

    void updateUniforms();
};