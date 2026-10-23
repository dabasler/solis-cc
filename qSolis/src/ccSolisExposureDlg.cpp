#include "ccSolisExposureDlg.h"

#include "SOLISVersion.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTime>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int QualityFast = 0;
constexpr int QualityNormal = 1;
constexpr int QualityHigh = 2;
constexpr int QualityCustom = 3;

struct QualityPreset
{
    int resolution;
    int skyRays;
};

constexpr QualityPreset QualityPresets[] = {
    {512, 64},
    {1024, 256},
    {2048, 512},
};

double dayOfYearWithTime(int day, const QTime& time)
{
    return static_cast<double>(day)
         + static_cast<double>(time.hour()) / 24.0
         + static_cast<double>(time.minute()) / 1440.0
         + static_cast<double>(time.second()) / 86400.0;
}

void setDayAndTime(double dayOfYear, QSpinBox* daySpinBox, QTimeEdit* timeEdit)
{
    if (!daySpinBox || !timeEdit || !std::isfinite(dayOfYear))
        return;

    const double dayFloor = std::floor(dayOfYear);
    const int day = std::max(1, std::min(366, static_cast<int>(dayFloor)));
    int seconds = static_cast<int>(std::lround((dayOfYear - dayFloor) * 86400.0));
    seconds = std::max(0, std::min(86399, seconds));

    daySpinBox->setValue(day);
    timeEdit->setTime(QTime(seconds / 3600,
                            (seconds % 3600) / 60,
                            seconds % 60));
}
} // namespace

ccSolisExposureDlg::ccSolisExposureDlg(QWidget* parent)
    : QDialog(parent, Qt::Tool)
{
    setupUi(this);

    const QString version = QString::fromLatin1(solis::Version);
    versionLabel->setText(tr("qSOLIS v%1").arg(version));
    setWindowTitle(tr("SOLIS Canopy Exposure v%1").arg(version));

    const auto update = [this]() { updateUi(); };
    connect(skyCheckBox, &QCheckBox::toggled, this, update);
    connect(sunCheckBox, &QCheckBox::toggled, this, update);
    connect(radiativeCheckBox, &QCheckBox::toggled, this, update);
    connect(contextComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateUi(); });
    connect(crownSummaryCheckBox, &QCheckBox::toggled, this,
            [this](bool enabled)
            {
                if (enabled)
                    radiativeCheckBox->setChecked(true);
                updateUi();
            });
    connect(voxelAutoCheckBox, &QCheckBox::toggled, this, update);
    connect(timeModeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateUi(); });
    connect(durationSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double) { updateUi(); });
    connect(accelCheckBox, &QCheckBox::toggled, this, update);
    connect(qualityComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index)
            {
                applyQualityPreset(index);
                updateUi();
            });
    connect(resolutionSpinBox, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int) { updateUi(); });
    connect(raysSpinBox, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int) { updateUi(); });
    connect(advancedButton, &QToolButton::toggled, this,
            [this](bool visible)
            {
                advancedPanel->setVisible(visible);
                advancedButton->setArrowType(visible ? Qt::DownArrow : Qt::RightArrow);
                QTimer::singleShot(0, this, [this]() { adjustSize(); });
            });

    qualityComboBox->setCurrentIndex(QualityNormal);
    applyQualityPreset(QualityNormal);
    updateUi();
}

void ccSolisExposureDlg::setOptions(const solis::ExposureOptions& options)
{
    skyCheckBox->setChecked(solis::ExposureCalculator::HasMetric(options, solis::ExposureSky));
    sunCheckBox->setChecked(solis::ExposureCalculator::HasMetric(options, solis::ExposureSun));
    radiativeCheckBox->setChecked(solis::ExposureCalculator::HasMetric(options, solis::ExposureRadiative));
    switch (options.occlusionContext)
    {
    case solis::ExposureOcclusionContext::TargetOnly: contextComboBox->setCurrentIndex(0); break;
    case solis::ExposureOcclusionContext::SelectedEntities: contextComboBox->setCurrentIndex(1); break;
    case solis::ExposureOcclusionContext::AllVisibleEntities: contextComboBox->setCurrentIndex(2); break;
    }

    crownSummaryCheckBox->setChecked(options.crownSummary);
    voxelAutoCheckBox->setChecked(options.crownVoxelAuto);
    voxelSizeSpinBox->setValue(options.crownVoxelSize > 0.0 ? options.crownVoxelSize : 0.2);
    thresholdSpinBox->setValue(options.crownThreshold);

    const auto& calculation = options.calculation;
    latSpinBox->setValue(calculation.site.latitudeDeg);
    lonSpinBox->setValue(calculation.site.longitudeDeg);
    elevSpinBox->setValue(calculation.site.elevationM);
    linkeSpinBox->setValue(calculation.site.linkeTurbidity);
    timeModeComboBox->setCurrentIndex(calculation.site.timeMode == solis::TimeMode::UTC ? 0 : 1);
    utcOffsetSpinBox->setValue(calculation.site.utcOffsetHours);

    setDayAndTime(calculation.time.dayOfYear, doySpinBox, timeEdit);
    durationSpinBox->setValue(calculation.time.mode == solis::TemporalMode::Instantaneous
                                  ? 0.0
                                  : calculation.time.durationHours);
    timestepSpinBox->setValue(calculation.time.timestepMinutes);

    resolutionSpinBox->setValue(static_cast<int>(calculation.render.resolution));
    depthSpinBox->setValue(calculation.render.depthBias);
    threadsSpinBox->setValue(static_cast<int>(calculation.render.cpuThreadCount));
    closedMeshCheckBox->setChecked(calculation.render.meshIsClosed);

    raysSpinBox->setValue(static_cast<int>(calculation.sampling.diffuseRayCount));
    accelCheckBox->setChecked(calculation.sampling.acceleratedDirectSampling);
    angleSpinBox->setValue(calculation.sampling.directAngularToleranceDeg);

    qualityComboBox->setCurrentIndex(presetForCurrentQuality());
    updateUi();
}

solis::ExposureOptions ccSolisExposureDlg::options() const
{
    solis::ExposureOptions options;
    options.metrics = solis::ExposureNone;
    if (skyCheckBox->isChecked())
        options.metrics |= solis::ExposureSky;
    if (sunCheckBox->isChecked())
        options.metrics |= solis::ExposureSun;
    if (radiativeCheckBox->isChecked())
        options.metrics |= solis::ExposureRadiative;

    switch (contextComboBox->currentIndex())
    {
    case 0: options.occlusionContext = solis::ExposureOcclusionContext::TargetOnly; break;
    case 2: options.occlusionContext = solis::ExposureOcclusionContext::AllVisibleEntities; break;
    default: options.occlusionContext = solis::ExposureOcclusionContext::SelectedEntities; break;
    }

    options.crownSummary = crownSummaryCheckBox->isChecked();
    options.crownVoxelAuto = voxelAutoCheckBox->isChecked();
    options.crownVoxelSize = voxelSizeSpinBox->value();
    options.crownThreshold = thresholdSpinBox->value();

    auto& calculation = options.calculation;
    calculation.site.latitudeDeg = latSpinBox->value();
    calculation.site.longitudeDeg = lonSpinBox->value();
    calculation.site.elevationM = elevSpinBox->value();
    calculation.site.linkeTurbidity = linkeSpinBox->value();
    calculation.site.timeMode = timeModeComboBox->currentIndex() == 0
                                    ? solis::TimeMode::UTC
                                    : solis::TimeMode::LocalFixedOffset;
    calculation.site.utcOffsetHours = utcOffsetSpinBox->value();

    calculation.time.dayOfYear = dayOfYearWithTime(doySpinBox->value(), timeEdit->time());
    calculation.time.durationHours = durationSpinBox->value();
    calculation.time.timestepMinutes = timestepSpinBox->value();
    calculation.time.mode = durationSpinBox->value() <= 0.0
                                ? solis::TemporalMode::Instantaneous
                                : solis::TemporalMode::IntegratedEnergy;
    calculation.time.energyUnit = solis::EnergyUnit::WhPerM2;

    calculation.spatialMode = solis::SpatialMode::HorizontalIrradiance;
    calculation.render.resolution = static_cast<unsigned>(resolutionSpinBox->value());
    calculation.render.depthBias = depthSpinBox->value();
    calculation.render.cpuThreadCount = static_cast<unsigned>(threadsSpinBox->value());
    calculation.render.parallelCpuProjection = calculation.render.cpuThreadCount != 1;
    calculation.render.meshIsClosed = closedMeshCheckBox->isChecked();

    calculation.sampling.diffuseRayCount = static_cast<unsigned>(raysSpinBox->value());
    calculation.sampling.acceleratedDirectSampling = accelCheckBox->isChecked();
    calculation.sampling.directAngularToleranceDeg = angleSpinBox->value();

    return options;
}

void ccSolisExposureDlg::setSelectionKinds(bool hasPointClouds, bool hasMeshes)
{
    m_hasPointClouds = hasPointClouds;
    closedMeshCheckBox->setEnabled(hasMeshes);
    if (!hasMeshes)
        closedMeshCheckBox->setChecked(false);

    if (hasPointClouds && hasMeshes)
    {
        weightingLabel->setText(
            tr("Point clouds use occupied-voxel weighting; meshes use surface-area weighting."));
    }
    else if (hasMeshes)
    {
        weightingLabel->setText(tr("Selected meshes use surface-area weighting."));
    }
    else
    {
        weightingLabel->setText(tr("Selected point clouds use occupied-voxel weighting."));
    }

    updateUi();
}

void ccSolisExposureDlg::setAdvancedVisible(bool visible)
{
    advancedButton->setChecked(visible);
    advancedPanel->setVisible(visible);
    advancedButton->setArrowType(visible ? Qt::DownArrow : Qt::RightArrow);
}

bool ccSolisExposureDlg::advancedVisible() const
{
    return advancedButton->isChecked();
}

void ccSolisExposureDlg::setQualityPresetIndex(int index)
{
    index = std::clamp(index, QualityFast, QualityCustom);
    qualityComboBox->setCurrentIndex(index);
    applyQualityPreset(index);
    updateUi();
}

int ccSolisExposureDlg::qualityPresetIndex() const
{
    return qualityComboBox->currentIndex();
}

void ccSolisExposureDlg::updateUi()
{
    if (crownSummaryCheckBox->isChecked() && !radiativeCheckBox->isChecked())
        radiativeCheckBox->setChecked(true);

    const bool temporal = sunCheckBox->isChecked() || radiativeCheckBox->isChecked();
    const bool interval = temporal && durationSpinBox->value() > 0.0;
    const bool radiative = radiativeCheckBox->isChecked();
    const bool crown = crownSummaryCheckBox->isChecked();
    const bool custom = qualityComboBox->currentIndex() == QualityCustom;
    const bool needsSkyRays = skyCheckBox->isChecked() || radiative;

    siteGroupBox->setEnabled(temporal);
    linkeLabel->setEnabled(radiative);
    linkeSpinBox->setEnabled(radiative);
    utcOffsetSpinBox->setEnabled(temporal && timeModeComboBox->currentIndex() != 0);
    timestepSpinBox->setEnabled(interval);
    timestepLabel->setEnabled(interval);

    voxelAutoCheckBox->setEnabled(crown && m_hasPointClouds);
    voxelSizeSpinBox->setEnabled(crown && m_hasPointClouds && !voxelAutoCheckBox->isChecked());
    thresholdSpinBox->setEnabled(crown);
    thresholdLabel->setEnabled(crown);

    resolutionSpinBox->setEnabled(custom);
    raysSpinBox->setEnabled(custom && needsSkyRays);
    raysLabel->setEnabled(needsSkyRays);

    accelCheckBox->setEnabled(interval);
    angleLabel->setEnabled(interval && accelCheckBox->isChecked());
    angleSpinBox->setEnabled(interval && accelCheckBox->isChecked());

    qualitySummaryLabel->setText(
        needsSkyRays
            ? tr("%1 px / %2 sky rays").arg(resolutionSpinBox->value()).arg(raysSpinBox->value())
            : tr("%1 px / sky rays not used").arg(resolutionSpinBox->value()));

    switch (contextComboBox->currentIndex())
    {
    case 0:
        contextHintLabel->setText(tr("Only each target itself blocks rays (self-exposure)."));
        break;
    case 2:
        contextHintLabel->setText(tr("Every visible cloud/mesh in the DB tree can block rays; results are written only to the selected target(s)."));
        break;
    default:
        contextHintLabel->setText(tr("Selected entities mutually occlude each other; results are written separately to each selected target."));
        break;
    }

    if (auto* ok = buttonBox->button(QDialogButtonBox::Ok))
    {
        ok->setEnabled(skyCheckBox->isChecked()
                       || sunCheckBox->isChecked()
                       || radiativeCheckBox->isChecked());
    }
}

void ccSolisExposureDlg::applyQualityPreset(int index)
{
    if (index < QualityFast || index > QualityHigh)
        return;

    resolutionSpinBox->setValue(QualityPresets[index].resolution);
    raysSpinBox->setValue(QualityPresets[index].skyRays);
}

int ccSolisExposureDlg::presetForCurrentQuality() const
{
    for (int index = QualityFast; index <= QualityHigh; ++index)
    {
        if (resolutionSpinBox->value() == QualityPresets[index].resolution
            && raysSpinBox->value() == QualityPresets[index].skyRays)
        {
            return index;
        }
    }
    return QualityCustom;
}
