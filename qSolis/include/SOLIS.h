#pragma once

#include "SOLISSolarModel.h"

#include <GenericCloud.h>
#include <GenericMesh.h>
#include <GenericProgressCallback.h>

#include <QString>

#include <vector>

class SOLISAccumulationBackend;

namespace solis
{
enum class TemporalMode { Instantaneous, MeanIrradiance, IntegratedEnergy };
enum class EnergyUnit { WhPerM2, KWhPerM2 };
enum class SpatialMode { Visibility, HorizontalIrradiance, SurfaceIrradiance };

struct TimeOptions
{
    TemporalMode mode = TemporalMode::Instantaneous;
    double dayOfYear = 172.5;
    double durationHours = 24.0;
    double timestepMinutes = 5.0;
    EnergyUnit energyUnit = EnergyUnit::WhPerM2;
};

struct RenderOptions
{
    unsigned resolution = 1024;
    bool meshIsClosed = false;
    double depthBias = 1.0e-3;
    bool parallelCpuProjection = true;
    unsigned cpuThreadCount = 0;
};

struct SamplingOptions
{
    bool acceleratedDirectSampling = false;
    double directAngularToleranceDeg = 0.1;
    unsigned diffuseRayCount = 256;
};

struct CalculationOptions
{
    Site site;
    TimeOptions time;
    RenderOptions render;
    SamplingOptions sampling;
    SpatialMode spatialMode = SpatialMode::HorizontalIrradiance;
};

struct RaySample
{
    CCVector3d direction;
    double irradianceWm2 = 0.0;
    double weightHours = 1.0;
};
}

class SOLIS
{
public:
    static bool ValidateOptions(const solis::CalculationOptions& options, QString* errorMessage = nullptr);
    static bool GenerateDirectSamples(const solis::CalculationOptions& options,
                                      std::vector<solis::RaySample>& samples);
    static bool GenerateDiffuseRays(unsigned count, std::vector<CCVector3d>& rays);
    static double AggregateDiffuseIrradiance(const solis::CalculationOptions& options);

    static bool LaunchDirect(const std::vector<solis::RaySample>& samples,
                             const solis::CalculationOptions& options,
                             CCCoreLib::GenericCloud* vertices,
                             CCCoreLib::GenericMesh* mesh = nullptr,
                             const std::vector<CCVector3d>* normals = nullptr,
                             CCCoreLib::GenericProgressCallback* progressCb = nullptr,
                             const QString& entityName = QString(),
                             SOLISAccumulationBackend* backend = nullptr);

    static bool LaunchDiffuse(const std::vector<CCVector3d>& rays,
                              double diffuseIrradiance,
                              const solis::CalculationOptions& options,
                              CCCoreLib::GenericCloud* vertices,
                              CCCoreLib::GenericMesh* mesh = nullptr,
                              const std::vector<CCVector3d>* normals = nullptr,
                              CCCoreLib::GenericProgressCallback* progressCb = nullptr,
                              const QString& entityName = QString(),
                              SOLISAccumulationBackend* backend = nullptr);

    static QString OutputUnit(const solis::CalculationOptions& options);

private:
    static double energyUnitScale(solis::EnergyUnit unit);
};
