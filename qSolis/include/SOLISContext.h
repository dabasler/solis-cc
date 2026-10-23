#pragma once

#include "SOLIS.h"
#include "SOLISBackend.h"

#include <GenericCloud.h>
#include <GenericMesh.h>

#include <cstdint>
#include <vector>

class QOpenGLBuffer;
class QOpenGLContext;
class QOpenGLFramebufferObject;
class QSurface;

class SOLISContext final : public SOLISAccumulationBackend
{
public:
    SOLISContext();
    ~SOLISContext() override;

    bool setOcclusionContext(const std::vector<SOLISOccluder>& occluders) override;

    bool init(unsigned W,
              unsigned H,
              CCCoreLib::GenericCloud* cloud,
              CCCoreLib::GenericMesh* mesh,
              bool closedMesh,
              double depthBias,
              bool parallelCpuProjection,
              unsigned cpuThreadCount) override;

    int visibility(std::vector<uint8_t>& visible,
                   const CCVector3d& lightTravelDirection) override;

    int accumulate(std::vector<double>& accumulator,
                   const CCVector3d& lightTravelDirection,
                   double scalarWeight,
                   solis::SpatialMode spatialMode,
                   const std::vector<CCVector3d>* normals) override;

private:
    void associateToEntity(CCCoreLib::GenericCloud* cloud, CCCoreLib::GenericMesh* mesh);
    bool buildGeometryCache();
    bool buildContextGeometryCache();
    void computeContextBoundingBox();
    bool glInit();
    bool makeCurrent();
    bool uploadGeometry();
    bool drawEntity();
    bool drawContext();
    bool projectDepthVisible(unsigned pointIndex,
                             const double* viewMatrix,
                             const double* projectionMatrix,
                             const int* viewport) const;

    CCCoreLib::GenericCloud* m_vertices = nullptr;
    CCCoreLib::GenericMesh* m_mesh = nullptr;
    CCVector3 m_entityCenter{0, 0, 0};
    PointCoordinateType m_entityDiagonal = 0;

    QSurface* m_glSurface = nullptr;
    QOpenGLContext* m_glContext = nullptr;
    QOpenGLFramebufferObject* m_framebuffer = nullptr;
    QOpenGLBuffer* m_pixelBuffer = nullptr;
    QOpenGLBuffer* m_vertexBuffer = nullptr;
    QOpenGLBuffer* m_indexBuffer = nullptr;

    unsigned m_width = 0;
    unsigned m_height = 0;
    bool m_meshIsClosed = false;
    double m_depthBias = 1.0e-3;
    bool m_parallelCpuProjection = true;
    unsigned m_cpuThreadCount = 1;

    std::vector<float> m_snapZ;
    std::vector<uint8_t> m_snapC;
    std::vector<uint8_t> m_visibilityScratch;
    std::vector<CCVector3d> m_points;
    std::vector<PointCoordinateType> m_drawVertices;
    std::vector<unsigned> m_indices;
    bool m_useIndexBuffer = false;
    unsigned m_drawVertexCount = 0;

    struct ContextDrawBatch
    {
        bool triangles = false;
        unsigned firstVertex = 0;
        unsigned vertexCount = 0;
    };
    std::vector<SOLISOccluder> m_occlusionContext;
    std::vector<ContextDrawBatch> m_contextDrawBatches;
    bool m_contextRendering = false;
};
