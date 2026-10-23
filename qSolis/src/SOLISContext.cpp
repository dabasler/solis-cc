#include "SOLISContext.h"

#include <CCMath.h>
#include <GenericIndexedMesh.h>
#include <GenericTriangle.h>
#include <ccIncludeGL.h>
#include <ccLog.h>

#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLFramebufferObject>
#include <QOpenGLVersionFunctionsFactory>
#include <QSurface>
#include <QSurfaceFormat>
#include <QWindow>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <new>
#include <thread>
#include <type_traits>

using namespace CCCoreLib;

namespace
{
GLenum coordinateGLType()
{
    return std::is_same<PointCoordinateType, double>::value ? GL_DOUBLE : GL_FLOAT;
}

unsigned automaticThreadCount(unsigned requested)
{
    if (requested != 0)
        return requested;
    const unsigned hc = std::thread::hardware_concurrency();
    return hc == 0 ? 1u : hc;
}
} // namespace

SOLISContext::SOLISContext() = default;

SOLISContext::~SOLISContext()
{
    if (m_glContext && m_glSurface)
        m_glContext->makeCurrent(m_glSurface);

    delete m_framebuffer;
    delete m_indexBuffer;
    delete m_vertexBuffer;
    delete m_pixelBuffer;

    if (m_glContext)
    {
        m_glContext->doneCurrent();
        delete m_glContext;
    }
    delete m_glSurface;
}

bool SOLISContext::setOcclusionContext(const std::vector<SOLISOccluder>& occluders)
{
    // Must be configured before init(), because geometry is uploaded once and reused.
    if (m_vertexBuffer || m_framebuffer || m_glContext)
        return false;

    m_occlusionContext.clear();
    try
    {
        for (const SOLISOccluder& occluder : occluders)
        {
            if (!occluder.cloud)
                continue;
            const auto duplicate = std::find_if(m_occlusionContext.begin(), m_occlusionContext.end(),
                                                [&](const SOLISOccluder& existing)
                                                {
                                                    return existing.cloud == occluder.cloud
                                                        && existing.mesh == occluder.mesh;
                                                });
            if (duplicate == m_occlusionContext.end())
                m_occlusionContext.push_back(occluder);
        }
    }
    catch (const std::bad_alloc&)
    {
        m_occlusionContext.clear();
        return false;
    }
    return true;
}

bool SOLISContext::init(unsigned W,
                        unsigned H,
                        GenericCloud* cloud,
                        GenericMesh* mesh,
                        bool closedMesh,
                        double depthBias,
                        bool parallelCpuProjection,
                        unsigned cpuThreadCount)
{
    if (!cloud || W == 0 || H == 0)
        return false;

    const size_t pixelCount = static_cast<size_t>(W) * static_cast<size_t>(H);
    if (pixelCount > static_cast<size_t>(std::numeric_limits<int>::max()) / 4u)
        return false;

    m_width = W;
    m_height = H;
    m_meshIsClosed = (closedMesh || !mesh);
    m_depthBias = std::max(0.0, std::min(0.05, depthBias));
    m_parallelCpuProjection = parallelCpuProjection;
    m_cpuThreadCount = automaticThreadCount(cpuThreadCount);

    m_contextRendering = !m_occlusionContext.empty();
    if (m_occlusionContext.size() == 1
        && m_occlusionContext.front().cloud == cloud
        && m_occlusionContext.front().mesh == mesh)
    {
        // A one-entity context is exactly the legacy self-occlusion problem. Keep
        // the original renderer path byte-for-byte for numerical compatibility and
        // to preserve the closed-mesh optimization.
        m_contextRendering = false;
    }
    try
    {
        m_snapZ.assign(pixelCount, 0.0f);
        m_snapC.clear();
        if (!m_contextRendering && mesh && !m_meshIsClosed)
            m_snapC.assign(pixelCount * 4u, 0u);
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }

    associateToEntity(cloud, mesh);
    if (m_contextRendering)
        computeContextBoundingBox();
    if (!(m_contextRendering ? buildContextGeometryCache() : buildGeometryCache())
        || !glInit()
        || !makeCurrent())
    {
        return false;
    }

    auto* functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(m_glContext);
    if (!functions)
        return false;
    functions->initializeOpenGLFunctions();

    // Render to an explicit FBO instead of the hidden QWindow's default
    // framebuffer.  The latter can be compositor/surface-size dependent and
    // was observed to return an empty depth image at 2048x2048 on Linux even
    // though 1024x1024 worked.  An FBO gives us a framebuffer whose dimensions
    // and depth attachment are independent of the native window system.
    GLint maxViewport[2] = {0, 0};
    GLint maxRenderbuffer = 0;
    GLint maxTexture = 0;
    functions->glGetIntegerv(GL_MAX_VIEWPORT_DIMS, maxViewport);
    functions->glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbuffer);
    functions->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture);

    if (static_cast<GLint>(m_width) > maxViewport[0]
        || static_cast<GLint>(m_height) > maxViewport[1]
        || static_cast<GLint>(m_width) > maxRenderbuffer
        || static_cast<GLint>(m_height) > maxRenderbuffer
        || static_cast<GLint>(m_width) > maxTexture
        || static_cast<GLint>(m_height) > maxTexture)
    {
        ccLog::Error(QStringLiteral("[qSOLIS] Requested render resolution %1x%2 exceeds OpenGL limits "
                                    "(viewport %3x%4, renderbuffer %5, texture %6)")
                         .arg(m_width)
                         .arg(m_height)
                         .arg(maxViewport[0])
                         .arg(maxViewport[1])
                         .arg(maxRenderbuffer)
                         .arg(maxTexture));
        return false;
    }

    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    fboFormat.setTextureTarget(GL_TEXTURE_2D);
    fboFormat.setInternalTextureFormat(GL_RGBA8);
    fboFormat.setSamples(0);

    m_framebuffer = new QOpenGLFramebufferObject(static_cast<int>(m_width),
                                                  static_cast<int>(m_height),
                                                  fboFormat);
    if (!m_framebuffer->isValid() || !m_framebuffer->bind())
    {
        ccLog::Error(QStringLiteral("[qSOLIS] Failed to create/bind %1x%2 offscreen framebuffer")
                         .arg(m_width)
                         .arg(m_height));
        return false;
    }

    GLint depthBits = 0;
    functions->glGetIntegerv(GL_DEPTH_BITS, &depthBits);
    if (depthBits <= 0)
    {
        ccLog::Error("[qSOLIS] Offscreen framebuffer has no depth attachment");
        m_framebuffer->release();
        return false;
    }

    functions->glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    functions->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    functions->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    m_pixelBuffer = new QOpenGLBuffer(QOpenGLBuffer::PixelPackBuffer);
    if (!m_pixelBuffer->create() || !m_pixelBuffer->bind())
        return false;
    m_pixelBuffer->setUsagePattern(QOpenGLBuffer::StreamRead);
    m_pixelBuffer->allocate(static_cast<int>(pixelCount * 4u)); // max(GL_DEPTH32F, RGBA8) = 4 bytes/pixel
    m_pixelBuffer->release();

    if (!uploadGeometry())
    {
        m_framebuffer->release();
        return false;
    }

    m_framebuffer->release();
    m_glContext->doneCurrent();
    return true;
}

void SOLISContext::associateToEntity(GenericCloud* cloud, GenericMesh* mesh)
{
    m_vertices = cloud;
    m_mesh = mesh;

    CCVector3 bbMin;
    CCVector3 bbMax;
    m_vertices->getBoundingBox(bbMin, bbMax);
    m_entityDiagonal = (bbMax - bbMin).norm();
    m_entityCenter = (bbMax + bbMin) / 2;
    if (!GreaterThanEpsilon(m_entityDiagonal))
        m_entityDiagonal = PC_ONE;
}

bool SOLISContext::buildGeometryCache()
{
    try
    {
        const unsigned pointCount = m_vertices->size();
        m_points.resize(pointCount);
        m_drawVertices.clear();
        m_indices.clear();

        m_vertices->placeIteratorAtBeginning();
        m_drawVertices.reserve(static_cast<size_t>(pointCount) * 3u);
        for (unsigned i = 0; i < pointCount; ++i)
        {
            const CCVector3* p = m_vertices->getNextPoint();
            if (!p)
                return false;
            m_points[i] = p->toDouble();
            m_drawVertices.push_back(p->x);
            m_drawVertices.push_back(p->y);
            m_drawVertices.push_back(p->z);
        }

        if (static_cast<size_t>(pointCount) > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
            return false;
        m_useIndexBuffer = false;
        m_drawVertexCount = pointCount;

        if (m_mesh)
        {
            if (auto* indexedMesh = dynamic_cast<GenericIndexedMesh*>(m_mesh))
            {
                const unsigned triangleCount = indexedMesh->size();
                if (static_cast<size_t>(triangleCount) * 3u > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
                    return false;
                m_indices.reserve(static_cast<size_t>(triangleCount) * 3u);
                indexedMesh->placeIteratorAtBeginning();
                for (unsigned i = 0; i < triangleCount; ++i)
                {
                    const VerticesIndexes* tri = indexedMesh->getNextTriangleVertIndexes();
                    if (!tri || tri->i1 >= pointCount || tri->i2 >= pointCount || tri->i3 >= pointCount)
                        return false;
                    m_indices.push_back(tri->i1);
                    m_indices.push_back(tri->i2);
                    m_indices.push_back(tri->i3);
                }
                m_useIndexBuffer = true;
            }
            else
            {
                // Rare generic-mesh fallback: cache flattened triangle coordinates once.
                m_drawVertices.clear();
                const unsigned triangleCount = m_mesh->size();
                if (static_cast<size_t>(triangleCount) * 3u > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
                    return false;
                m_drawVertices.reserve(static_cast<size_t>(triangleCount) * 9u);
                m_mesh->placeIteratorAtBeginning();
                for (unsigned i = 0; i < triangleCount; ++i)
                {
                    const GenericTriangle* tri = m_mesh->_getNextTriangle();
                    if (!tri)
                        return false;
                    const CCVector3* verts[3] = {tri->_getA(), tri->_getB(), tri->_getC()};
                    for (const CCVector3* v : verts)
                    {
                        m_drawVertices.push_back(v->x);
                        m_drawVertices.push_back(v->y);
                        m_drawVertices.push_back(v->z);
                    }
                }
                m_drawVertexCount = triangleCount * 3u;
            }
        }
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    return true;
}

void SOLISContext::computeContextBoundingBox()
{
    bool haveBounds = false;
    CCVector3 bbMin;
    CCVector3 bbMax;

    for (const SOLISOccluder& occluder : m_occlusionContext)
    {
        if (!occluder.cloud || occluder.cloud->size() == 0)
            continue;

        CCVector3 localMin;
        CCVector3 localMax;
        occluder.cloud->getBoundingBox(localMin, localMax);
        if (!haveBounds)
        {
            bbMin = localMin;
            bbMax = localMax;
            haveBounds = true;
        }
        else
        {
            bbMin.x = std::min(bbMin.x, localMin.x);
            bbMin.y = std::min(bbMin.y, localMin.y);
            bbMin.z = std::min(bbMin.z, localMin.z);
            bbMax.x = std::max(bbMax.x, localMax.x);
            bbMax.y = std::max(bbMax.y, localMax.y);
            bbMax.z = std::max(bbMax.z, localMax.z);
        }
    }

    if (!haveBounds)
        return;

    m_entityDiagonal = (bbMax - bbMin).norm();
    m_entityCenter = (bbMax + bbMin) / 2;
    if (!GreaterThanEpsilon(m_entityDiagonal))
        m_entityDiagonal = PC_ONE;
}

bool SOLISContext::buildContextGeometryCache()
{
    try
    {
        const unsigned targetPointCount = m_vertices->size();
        m_points.resize(targetPointCount);
        m_vertices->placeIteratorAtBeginning();
        for (unsigned i = 0; i < targetPointCount; ++i)
        {
            const CCVector3* p = m_vertices->getNextPoint();
            if (!p)
                return false;
            m_points[i] = p->toDouble();
        }

        m_drawVertices.clear();
        m_indices.clear();
        m_contextDrawBatches.clear();
        m_useIndexBuffer = false;
        m_drawVertexCount = 0;

        for (const SOLISOccluder& occluder : m_occlusionContext)
        {
            if (!occluder.cloud || occluder.cloud->size() == 0)
                continue;

            ContextDrawBatch batch;
            batch.firstVertex = static_cast<unsigned>(m_drawVertices.size() / 3u);
            batch.triangles = (occluder.mesh != nullptr);

            if (occluder.mesh)
            {
                const unsigned triangleCount = occluder.mesh->size();
                if (static_cast<size_t>(triangleCount) * 3u > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
                    return false;
                m_drawVertices.reserve(m_drawVertices.size() + static_cast<size_t>(triangleCount) * 9u);
                occluder.mesh->placeIteratorAtBeginning();
                for (unsigned i = 0; i < triangleCount; ++i)
                {
                    const GenericTriangle* tri = occluder.mesh->_getNextTriangle();
                    if (!tri)
                        return false;
                    const CCVector3* verts[3] = {tri->_getA(), tri->_getB(), tri->_getC()};
                    for (const CCVector3* v : verts)
                    {
                        m_drawVertices.push_back(v->x);
                        m_drawVertices.push_back(v->y);
                        m_drawVertices.push_back(v->z);
                    }
                }
                batch.vertexCount = triangleCount * 3u;
            }
            else
            {
                const unsigned pointCount = occluder.cloud->size();
                if (static_cast<size_t>(pointCount) > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
                    return false;
                m_drawVertices.reserve(m_drawVertices.size() + static_cast<size_t>(pointCount) * 3u);
                occluder.cloud->placeIteratorAtBeginning();
                for (unsigned i = 0; i < pointCount; ++i)
                {
                    const CCVector3* point = occluder.cloud->getNextPoint();
                    if (!point)
                        return false;
                    m_drawVertices.push_back(point->x);
                    m_drawVertices.push_back(point->y);
                    m_drawVertices.push_back(point->z);
                }
                batch.vertexCount = pointCount;
            }

            if (batch.vertexCount != 0)
                m_contextDrawBatches.push_back(batch);
        }

        if (m_contextDrawBatches.empty() || m_drawVertices.empty())
            return false;
        if (m_drawVertices.size() / 3u > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
            return false;

        m_drawVertexCount = static_cast<unsigned>(m_drawVertices.size() / 3u);
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    return true;
}

bool SOLISContext::glInit()
{
    m_glContext = new QOpenGLContext;

    // A QWindow created only for offscreen rendering is not guaranteed to receive
    // a depth buffer on every Qt/platform combination unless one is requested.
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    if (format.depthBufferSize() < 24)
        format.setDepthBufferSize(24);
    m_glContext->setFormat(format);

    if (!m_glContext->create())
        return false;

    auto* surface = new QWindow;
    surface->setSurfaceType(QSurface::OpenGLSurface);
    surface->setFormat(m_glContext->format());
    surface->create();
    surface->resize(static_cast<int>(m_width), static_cast<int>(m_height));
    m_glSurface = surface;
    return true;
}

bool SOLISContext::makeCurrent()
{
    return m_glContext && m_glSurface && m_glContext->makeCurrent(m_glSurface);
}

bool SOLISContext::uploadGeometry()
{
    if (m_drawVertices.empty())
        return false;

    const size_t vertexBytes = m_drawVertices.size() * sizeof(PointCoordinateType);
    if (vertexBytes > static_cast<size_t>(std::numeric_limits<int>::max()))
        return false;

    m_vertexBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (!m_vertexBuffer->create() || !m_vertexBuffer->bind())
        return false;
    m_vertexBuffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vertexBuffer->allocate(m_drawVertices.data(), static_cast<int>(vertexBytes));
    m_vertexBuffer->release();

    if (m_useIndexBuffer)
    {
        const size_t indexBytes = m_indices.size() * sizeof(unsigned);
        if (indexBytes > static_cast<size_t>(std::numeric_limits<int>::max()))
            return false;
        m_indexBuffer = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
        if (!m_indexBuffer->create() || !m_indexBuffer->bind())
            return false;
        m_indexBuffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_indexBuffer->allocate(m_indices.data(), static_cast<int>(indexBytes));
        m_indexBuffer->release();
    }
    return true;
}

bool SOLISContext::drawEntity()
{
    if (m_contextRendering)
        return drawContext();

    if (!m_vertexBuffer || !m_vertexBuffer->bind())
        return false;

    auto* functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(m_glContext);
    if (!functions)
    {
        m_vertexBuffer->release();
        return false;
    }

    functions->glEnableClientState(GL_VERTEX_ARRAY);
    functions->glVertexPointer(3, coordinateGLType(), 0, nullptr);

    if (m_mesh)
    {
        if (m_useIndexBuffer)
        {
            if (!m_indexBuffer || !m_indexBuffer->bind())
            {
                functions->glDisableClientState(GL_VERTEX_ARRAY);
                m_vertexBuffer->release();
                return false;
            }
            functions->glDrawElements(GL_TRIANGLES,
                                      static_cast<GLsizei>(m_indices.size()),
                                      GL_UNSIGNED_INT,
                                      nullptr);
            m_indexBuffer->release();
        }
        else
        {
            functions->glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_drawVertexCount));
        }
    }
    else
    {
        functions->glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_drawVertexCount));
    }

    functions->glDisableClientState(GL_VERTEX_ARRAY);
    m_vertexBuffer->release();
    return true;
}

bool SOLISContext::drawContext()
{
    if (!m_vertexBuffer || !m_vertexBuffer->bind())
        return false;

    auto* functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(m_glContext);
    if (!functions)
    {
        m_vertexBuffer->release();
        return false;
    }

    functions->glEnableClientState(GL_VERTEX_ARRAY);
    functions->glVertexPointer(3, coordinateGLType(), 0, nullptr);
    for (const ContextDrawBatch& batch : m_contextDrawBatches)
    {
        functions->glDrawArrays(batch.triangles ? GL_TRIANGLES : GL_POINTS,
                                static_cast<GLint>(batch.firstVertex),
                                static_cast<GLsizei>(batch.vertexCount));
    }
    functions->glDisableClientState(GL_VERTEX_ARRAY);
    m_vertexBuffer->release();
    return true;
}

bool SOLISContext::projectDepthVisible(unsigned pointIndex,
                                       const double* viewMatrix,
                                       const double* projectionMatrix,
                                       const int* viewport) const
{
    CCVector3d p2d;
    ccGL::Project<double, double>(m_points[pointIndex], viewMatrix, projectionMatrix, viewport, p2d);

    const int x = static_cast<int>(std::floor(p2d.x));
    const int y = static_cast<int>(std::floor(p2d.y));
    if (x < 0 || y < 0 || x >= static_cast<int>(m_width) || y >= static_cast<int>(m_height))
        return false;

    const size_t dec = static_cast<size_t>(x) + static_cast<size_t>(y) * m_width;

    if (!m_contextRendering && m_mesh && !m_meshIsClosed)
    {
        uint8_t col = m_snapC[dec * 4u];
        if (x + 1 < static_cast<int>(m_width))
            col = std::max(col, m_snapC[(dec + 1u) * 4u]);
        if (y + 1 < static_cast<int>(m_height))
        {
            const size_t below = dec + m_width;
            col = std::max(col, m_snapC[below * 4u]);
            if (x + 1 < static_cast<int>(m_width))
                col = std::max(col, m_snapC[(below + 1u) * 4u]);
        }
        if (col == 0)
            return false;
    }

    return p2d.z < static_cast<double>(m_snapZ[dec]);
}

int SOLISContext::visibility(std::vector<uint8_t>& visible,
                             const CCVector3d& lightTravelDirection)
{
    if (!m_pixelBuffer || !m_pixelBuffer->isCreated() || !m_vertices)
        return -1;
    if (!makeCurrent())
        return -2;

    try
    {
        visible.assign(m_points.size(), 0u);
    }
    catch (const std::bad_alloc&)
    {
        m_glContext->doneCurrent();
        return -1;
    }

    const auto fail = [this](int code)
    {
        if (m_framebuffer)
            m_framebuffer->release();
        m_glContext->doneCurrent();
        return code;
    };

    auto* functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(m_glContext);
    if (!functions)
        return fail(-3);
    if (!m_framebuffer || !m_framebuffer->isValid() || !m_framebuffer->bind())
        return fail(-7);

    CCVector3d viewDir = lightTravelDirection;
    const double n2 = viewDir.norm2();
    if (n2 <= 1.0e-20)
        return fail(-1);
    viewDir /= std::sqrt(n2);

    functions->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    functions->glClearDepth(1.0);
    functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    functions->glDepthRange(std::min(0.25, 2.0 * m_depthBias), 1.0);
    functions->glPointSize(1.0f);
    functions->glDisable(GL_BLEND);
    functions->glEnable(GL_DEPTH_TEST);
    functions->glEnable(GL_CULL_FACE);
    functions->glDisable(GL_LIGHTING);
    functions->glDepthMask(GL_TRUE);

    double projectionMat[16];
    functions->glMatrixMode(GL_PROJECTION);
    functions->glLoadIdentity();
    const double halfDiagonal = std::max<double>(m_entityDiagonal / 2.0, 1.0e-6);
    double xMax = halfDiagonal;
    double yMax = halfDiagonal;
    if (m_width > m_height)
        xMax *= static_cast<double>(m_width) / static_cast<double>(m_height);
    else if (m_height > m_width)
        yMax *= static_cast<double>(m_height) / static_cast<double>(m_width);
    const double zMax = std::max<double>(m_entityDiagonal + 2.0, 2.0);
    functions->glOrtho(-xMax, xMax, -yMax, yMax, -zMax, zMax);
    functions->glGetDoublev(GL_PROJECTION_MATRIX, projectionMat);

    CCVector3d up(0.0, 0.0, 1.0);
    if (1.0 - std::abs(viewDir.dot(up)) < 1.0e-4)
        up = CCVector3d(0.0, 1.0, 0.0);
    const ccGLMatrixd viewMat = ccGL::LookAt(m_entityCenter.toDouble() - viewDir,
                                             m_entityCenter.toDouble(),
                                             up);
    functions->glMatrixMode(GL_MODELVIEW);
    functions->glLoadIdentity();
    functions->glMultMatrixd(viewMat.data());
    functions->glColor3ub(255, 255, 0);

    const GLboolean drawColor = (!m_contextRendering && m_mesh && !m_meshIsClosed) ? GL_TRUE : GL_FALSE;
    functions->glColorMask(drawColor, drawColor, drawColor, drawColor);

    int viewport[4]{0, 0, static_cast<int>(m_width), static_cast<int>(m_height)};
    functions->glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    if (m_contextRendering)
    {
        // Exposure context geometry is an occlusion shell: triangles block from
        // either side and point clouds retain the legacy one-pixel footprint.
        functions->glDisable(GL_CULL_FACE);
        if (!drawEntity())
            return fail(-6);
    }
    else
    {
        functions->glEnable(GL_CULL_FACE);
        functions->glCullFace(GL_BACK);
        if (!drawEntity())
            return fail(-6);
        if (m_mesh && !m_meshIsClosed)
        {
            functions->glCullFace(GL_FRONT);
            if (!drawEntity())
                return fail(-6);

            if (!m_pixelBuffer->bind())
                return fail(-4);
            functions->glReadPixels(0, 0, viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            if (!m_pixelBuffer->read(0, m_snapC.data(), static_cast<int>(m_snapC.size())))
            {
                m_pixelBuffer->release();
                return fail(-5);
            }
            m_pixelBuffer->release();
        }
    }

    if (!m_pixelBuffer->bind())
        return fail(-4);
    functions->glReadPixels(0, 0, viewport[2], viewport[3], GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    if (!m_pixelBuffer->read(0, m_snapZ.data(), static_cast<int>(m_snapZ.size() * sizeof(float))))
    {
        m_pixelBuffer->release();
        return fail(-5);
    }
    m_pixelBuffer->release();
    functions->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    const unsigned n = static_cast<unsigned>(m_points.size());
    const unsigned threadCount = (m_parallelCpuProjection && n >= 50000)
                               ? std::max(1u, std::min(m_cpuThreadCount, n / 10000u))
                               : 1u;

    std::atomic<int> visibleCount{0};
    auto worker = [&](unsigned begin, unsigned end)
    {
        int localCount = 0;
        for (unsigned i = begin; i < end; ++i)
        {
            if (projectDepthVisible(i, viewMat.data(), projectionMat, viewport))
            {
                visible[i] = 1u;
                ++localCount;
            }
        }
        visibleCount.fetch_add(localCount, std::memory_order_relaxed);
    };

    if (threadCount == 1)
    {
        worker(0, n);
    }
    else
    {
        std::vector<std::thread> threads;
        threads.reserve(threadCount);
        const unsigned block = (n + threadCount - 1u) / threadCount;
        for (unsigned t = 0; t < threadCount; ++t)
        {
            const unsigned begin = t * block;
            const unsigned end = std::min(n, begin + block);
            if (begin < end)
                threads.emplace_back(worker, begin, end);
        }
        for (auto& thread : threads)
            thread.join();
    }

    const int resultVisible = visibleCount.load(std::memory_order_relaxed);
    m_framebuffer->release();
    m_glContext->doneCurrent();
    return resultVisible;
}

int SOLISContext::accumulate(std::vector<double>& accumulator,
                             const CCVector3d& lightTravelDirection,
                             double scalarWeight,
                             solis::SpatialMode spatialMode,
                             const std::vector<CCVector3d>* normals)
{
    if (accumulator.size() != m_points.size())
        return -1;
    if (spatialMode == solis::SpatialMode::SurfaceIrradiance
        && (!normals || normals->size() != m_points.size()))
        return -1;

    const int visibleCount = visibility(m_visibilityScratch, lightTravelDirection);
    if (visibleCount < 0)
        return visibleCount;

    CCVector3d viewDir = lightTravelDirection;
    const double n2 = viewDir.norm2();
    if (n2 <= 1.0e-20)
        return -1;
    viewDir /= std::sqrt(n2);
    const CCVector3d surfaceToSource = -viewDir;

    const unsigned n = static_cast<unsigned>(m_visibilityScratch.size());
    const unsigned threadCount = (m_parallelCpuProjection && n >= 50000)
                               ? std::max(1u, std::min(m_cpuThreadCount, n / 10000u))
                               : 1u;
    auto worker = [&](unsigned begin, unsigned end)
    {
        for (unsigned i = begin; i < end; ++i)
        {
            if (!m_visibilityScratch[i])
                continue;
            double incidence = 1.0;
            if (spatialMode == solis::SpatialMode::HorizontalIrradiance)
                incidence = std::max(0.0, surfaceToSource.z);
            else if (spatialMode == solis::SpatialMode::SurfaceIrradiance)
                incidence = std::max(0.0, (*normals)[i].dot(surfaceToSource));
            accumulator[i] += scalarWeight * incidence;
        }
    };

    if (threadCount == 1)
    {
        worker(0, n);
    }
    else
    {
        std::vector<std::thread> threads;
        threads.reserve(threadCount);
        const unsigned block = (n + threadCount - 1u) / threadCount;
        for (unsigned t = 0; t < threadCount; ++t)
        {
            const unsigned begin = t * block;
            const unsigned end = std::min(n, begin + block);
            if (begin < end)
                threads.emplace_back(worker, begin, end);
        }
        for (auto& thread : threads)
            thread.join();
    }

    return visibleCount;
}


std::unique_ptr<SOLISAccumulationBackend> CreateSOLISAccumulationBackend()
{
    return std::make_unique<SOLISContext>();
}
