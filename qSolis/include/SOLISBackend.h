#pragma once

#include <CCGeom.h>
#include <GenericCloud.h>
#include <GenericMesh.h>

#include <memory>
#include <vector>

namespace solis { enum class SpatialMode; }

class SOLISAccumulationBackend
{
public:
    virtual ~SOLISAccumulationBackend() = default;
    virtual bool init(unsigned width,
                      unsigned height,
                      CCCoreLib::GenericCloud* cloud,
                      CCCoreLib::GenericMesh* mesh,
                      bool closedMesh,
                      double depthBias,
                      bool parallelCpuProjection,
                      unsigned cpuThreadCount) = 0;

    virtual int accumulate(std::vector<double>& accumulator,
                           const CCVector3d& lightTravelDirection,
                           double scalarWeight,
                           solis::SpatialMode spatialMode,
                           const std::vector<CCVector3d>* normals) = 0;
};

std::unique_ptr<SOLISAccumulationBackend> CreateSOLISAccumulationBackend();
