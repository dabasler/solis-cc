#pragma once

#include <CCGeom.h>
#include <GenericCloud.h>
#include <GenericMesh.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace solis { enum class SpatialMode; }

//! One geometry entity that participates in visibility/shadow rendering.
//! The target cloud is projected separately; this list only defines occluding geometry.
struct SOLISOccluder
{
    CCCoreLib::GenericCloud* cloud = nullptr;
    CCCoreLib::GenericMesh* mesh = nullptr;
};

class SOLISAccumulationBackend
{
public:
    virtual ~SOLISAccumulationBackend() = default;
    //! Optional exposure-only occlusion context. Call before init().
    //! When empty, init() keeps the legacy single-entity rendering path.
    virtual bool setOcclusionContext(const std::vector<SOLISOccluder>& occluders) = 0;

    virtual bool init(unsigned width,
                      unsigned height,
                      CCCoreLib::GenericCloud* cloud,
                      CCCoreLib::GenericMesh* mesh,
                      bool closedMesh,
                      double depthBias,
                      bool parallelCpuProjection,
                      unsigned cpuThreadCount) = 0;

    //! Renders one source direction and returns per-point geometric visibility (0/1).
    //! This is the reusable primitive used by exposure metrics so one FBO pass can
    //! feed multiple accumulators.
    virtual int visibility(std::vector<uint8_t>& visible,
                           const CCVector3d& lightTravelDirection) = 0;

    virtual int accumulate(std::vector<double>& accumulator,
                           const CCVector3d& lightTravelDirection,
                           double scalarWeight,
                           solis::SpatialMode spatialMode,
                           const std::vector<CCVector3d>* normals) = 0;
};

std::unique_ptr<SOLISAccumulationBackend> CreateSOLISAccumulationBackend();
