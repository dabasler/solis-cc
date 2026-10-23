//##########################################################################
//#                                                                        #
//#                       CLOUDCOMPARE PLUGIN: qSOLIS                      #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 or later of the License.      #
//#                                                                        #
//#                  COPYRIGHT: David Basler                               #
//#                                                                        #
//##########################################################################

#include "ccSolisDlg.h"
#include "SOLISVersion.h"

#include <QComboBox>
#include <QStandardItemModel>
#include <QTime>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int TemporalInstant = 0;
constexpr int TemporalMean = 1;
constexpr int TemporalEnergy = 2;

constexpr int SpatialHorizontal = 0;
constexpr int SpatialSurface = 1;
constexpr int SpatialVisibility = 2;

constexpr int TimeUTC = 0;
constexpr int TimeLocal = 1;

constexpr int UnitWh = 0;
constexpr int UnitKWh = 1;

constexpr int QualityFast = 0;
constexpr int QualityNormal = 1;
constexpr int QualityHigh = 2;
constexpr int QualityCustom = 3;

struct QualityPreset
{
    int resolution;
    int diffuseRays;
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

void setDayAndTime(double dayOfYear, QSpinBox* daySpin, QTimeEdit* timeEdit)
{
    if (!daySpin || !timeEdit || !std::isfinite(dayOfYear))
        return;

    const double dayFloor = std::floor(dayOfYear);
    const int day = std::max(1, std::min(366, static_cast<int>(dayFloor)));
    double fraction = std::max(0.0, dayOfYear - dayFloor);
    int seconds = static_cast<int>(std::lround(fraction * 86400.0));
    seconds = std::max(0, std::min(86399, seconds));

    daySpin->setValue(day);
    timeEdit->setTime(QTime(seconds / 3600, (seconds % 3600) / 60, seconds % 60));
}

} // namespace

ccSolisDlg::ccSolisDlg(QWidget* parent)
    : QDialog(parent, Qt::Tool)
    , Ui::SOLISDialog()
{
    setupUi(this);

    const QString version = QString::fromLatin1(solis::Version);
    versionLabel->setText(tr("qSOLIS v%1").arg(version));
    setWindowTitle(tr("SOLIS v%1").arg(version));

    connect(batchGroupBox, &QGroupBox::toggled, this, [this](bool) { updateBatchPreview(); });
    connect(doySpinBox, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int value)
            {
                if (!batchGroupBox->isChecked())
                    batchEndDoySpinBox->setValue(value);
                updateBatchPreview();
            });
    connect(timeEdit, &QTimeEdit::timeChanged, this, [this](const QTime&) { updateBatchPreview(); });
    connect(batchEndDoySpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { updateBatchPreview(); });
    connect(batchEndTimeEdit, &QTimeEdit::timeChanged, this, [this](const QTime&) { updateBatchPreview(); });
    connect(batchStepMinutesSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { updateBatchPreview(); });

    connect(temporalModeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updateDynamicUi(); });
    connect(spatialModeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updateDynamicUi(); });
    connect(timeModeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updateDynamicUi(); });
    connect(energyUnitComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updateDynamicUi(); });
    connect(calcDirectCheckBox, &QCheckBox::toggled, this, [this](bool) { updateDynamicUi(); });
    connect(calcDiffCheckBox, &QCheckBox::toggled, this, [this](bool) { updateDynamicUi(); });
    connect(directAccelerationCheckBox, &QCheckBox::toggled, this, [this](bool) { updateDynamicUi(); });

    connect(qualityPresetComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index)
            {
                applyQualityPreset(index);
                updateDynamicUi();
            });
    connect(resSpinBox, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int)
            {
                if (qualityPresetComboBox->currentIndex() == QualityCustom)
                    updateDynamicUi();
            });
    connect(raysSpinBox, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int)
            {
                if (qualityPresetComboBox->currentIndex() == QualityCustom)
                    updateDynamicUi();
            });

    connect(advancedToolButton, &QToolButton::toggled, this,
            [this](bool checked)
            {
                advancedPanel->setVisible(checked);
                advancedToolButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
                QTimer::singleShot(0, this, [this]() { adjustSize(); });
            });

    // Normal is the recommended default quality preset.
    qualityPresetComboBox->setCurrentIndex(QualityNormal);
    applyQualityPreset(QualityNormal);
    setAdvancedVisible(false);
    updateDynamicUi();
    updateBatchPreview();
}

void ccSolisDlg::setCalculationOptions(const solis::CalculationOptions& options)
{
    latDoubleSpinBox->setValue(options.site.latitudeDeg);
    lonDoubleSpinBox->setValue(options.site.longitudeDeg);
    elevationSpinBox->setValue(static_cast<int>(std::lround(options.site.elevationM)));
    linkeTurbidityDoubleSpinBox->setValue(options.site.linkeTurbidity);
    timeModeComboBox->setCurrentIndex(options.site.timeMode == solis::TimeMode::UTC ? TimeUTC : TimeLocal);
    utcOffsetDoubleSpinBox->setValue(options.site.utcOffsetHours);

    setDayAndTime(options.time.dayOfYear, doySpinBox, timeEdit);
    switch (options.time.mode)
    {
    case solis::TemporalMode::Instantaneous: temporalModeComboBox->setCurrentIndex(TemporalInstant); break;
    case solis::TemporalMode::MeanIrradiance: temporalModeComboBox->setCurrentIndex(TemporalMean); break;
    case solis::TemporalMode::IntegratedEnergy: temporalModeComboBox->setCurrentIndex(TemporalEnergy); break;
    }
    integrationDoubleSpinBox->setValue(options.time.durationHours);
    timestepDoubleSpinBox->setValue(options.time.timestepMinutes);
    energyUnitComboBox->setCurrentIndex(options.time.energyUnit == solis::EnergyUnit::KWhPerM2 ? UnitKWh : UnitWh);

    switch (options.spatialMode)
    {
    case solis::SpatialMode::HorizontalIrradiance: spatialModeComboBox->setCurrentIndex(SpatialHorizontal); break;
    case solis::SpatialMode::SurfaceIrradiance: spatialModeComboBox->setCurrentIndex(SpatialSurface); break;
    case solis::SpatialMode::Visibility: spatialModeComboBox->setCurrentIndex(SpatialVisibility); break;
    }

    resSpinBox->setValue(static_cast<int>(options.render.resolution));
    closedMeshCheckBox->setChecked(options.render.meshIsClosed);
    depthBiasDoubleSpinBox->setValue(options.render.depthBias);
    threadsSpinBox->setValue(static_cast<int>(options.render.cpuThreadCount));

    raysSpinBox->setValue(static_cast<int>(options.sampling.diffuseRayCount));
    directAccelerationCheckBox->setChecked(options.sampling.acceleratedDirectSampling);
    angularToleranceDoubleSpinBox->setValue(options.sampling.directAngularToleranceDeg);

    qualityPresetComboBox->setCurrentIndex(presetForCurrentQuality());
    updateDynamicUi();
    updateBatchPreview();
}

solis::CalculationOptions ccSolisDlg::calculationOptions() const
{
    solis::CalculationOptions options;
    options.site.latitudeDeg = latDoubleSpinBox->value();
    options.site.longitudeDeg = lonDoubleSpinBox->value();
    options.site.elevationM = static_cast<double>(elevationSpinBox->value());
    options.site.linkeTurbidity = linkeTurbidityDoubleSpinBox->value();
    options.site.timeMode = timeModeComboBox->currentIndex() == TimeLocal
                          ? solis::TimeMode::LocalFixedOffset
                          : solis::TimeMode::UTC;
    options.site.utcOffsetHours = utcOffsetDoubleSpinBox->value();

    options.time.dayOfYear = dayOfYearWithTime(doySpinBox->value(), timeEdit->time());
    switch (temporalModeComboBox->currentIndex())
    {
    case TemporalMean: options.time.mode = solis::TemporalMode::MeanIrradiance; break;
    case TemporalEnergy: options.time.mode = solis::TemporalMode::IntegratedEnergy; break;
    default: options.time.mode = solis::TemporalMode::Instantaneous; break;
    }
    options.time.durationHours = integrationDoubleSpinBox->value();
    options.time.timestepMinutes = timestepDoubleSpinBox->value();
    options.time.energyUnit = energyUnitComboBox->currentIndex() == UnitKWh
                            ? solis::EnergyUnit::KWhPerM2
                            : solis::EnergyUnit::WhPerM2;

    switch (spatialModeComboBox->currentIndex())
    {
    case SpatialSurface: options.spatialMode = solis::SpatialMode::SurfaceIrradiance; break;
    case SpatialVisibility: options.spatialMode = solis::SpatialMode::Visibility; break;
    default: options.spatialMode = solis::SpatialMode::HorizontalIrradiance; break;
    }

    options.render.resolution = static_cast<unsigned>(resSpinBox->value());
    options.render.meshIsClosed = closedMeshCheckBox->isChecked();
    options.render.depthBias = depthBiasDoubleSpinBox->value();
    options.render.cpuThreadCount = static_cast<unsigned>(threadsSpinBox->value());
    options.render.parallelCpuProjection = options.render.cpuThreadCount != 1;

    options.sampling.diffuseRayCount = static_cast<unsigned>(raysSpinBox->value());
    options.sampling.acceleratedDirectSampling = directAccelerationCheckBox->isChecked();
    options.sampling.directAngularToleranceDeg = angularToleranceDoubleSpinBox->value();
    return options;
}

void ccSolisDlg::setBatchOptions(const solis::BatchOptions& options)
{
    batchGroupBox->setChecked(options.enabled);
    setDayAndTime(options.endDayOfYear, batchEndDoySpinBox, batchEndTimeEdit);
    batchStepMinutesSpinBox->setValue(options.stepMinutes);
    updateBatchPreview();
}

solis::BatchOptions ccSolisDlg::batchOptions() const
{
    solis::BatchOptions options;
    options.enabled = batchGroupBox->isChecked();
    options.endDayOfYear = dayOfYearWithTime(batchEndDoySpinBox->value(), batchEndTimeEdit->time());
    options.stepMinutes = batchStepMinutesSpinBox->value();
    return options;
}

void ccSolisDlg::setComponents(bool direct, bool diffuse)
{
    calcDirectCheckBox->setChecked(direct);
    calcDiffCheckBox->setChecked(diffuse);
    updateDynamicUi();
}

bool ccSolisDlg::calculateDirect() const
{
    return calcDirectCheckBox->isChecked();
}

bool ccSolisDlg::calculateDiffuse() const
{
    return calcDiffCheckBox->isChecked();
}

void ccSolisDlg::setSurfaceModeAvailable(bool available, const QString& reason)
{
    if (auto* model = qobject_cast<QStandardItemModel*>(spatialModeComboBox->model()))
    {
        if (QStandardItem* item = model->item(SpatialSurface))
            item->setEnabled(available);
    }

    if (!available && spatialModeComboBox->currentIndex() == SpatialSurface)
        spatialModeComboBox->setCurrentIndex(SpatialHorizontal);

    surfaceWarningLabel->setVisible(!available);
    surfaceWarningLabel->setText(reason.isEmpty()
                                     ? tr("Surface mode is unavailable because one or more selected entities has no normals.")
                                     : reason);
    updateDynamicUi();
}

void ccSolisDlg::setHasMeshes(bool hasMeshes)
{
    closedMeshCheckBox->setEnabled(hasMeshes);
    if (!hasMeshes)
        closedMeshCheckBox->setChecked(false);
}

void ccSolisDlg::setQualityPresetIndex(int index)
{
    if (index < QualityFast || index > QualityCustom)
        index = QualityCustom;
    qualityPresetComboBox->setCurrentIndex(index);
    applyQualityPreset(index);
    updateDynamicUi();
}

int ccSolisDlg::qualityPresetIndex() const
{
    return qualityPresetComboBox->currentIndex();
}

void ccSolisDlg::setAdvancedVisible(bool visible)
{
    advancedToolButton->setChecked(visible);
    advancedPanel->setVisible(visible);
    advancedToolButton->setArrowType(visible ? Qt::DownArrow : Qt::RightArrow);
}

bool ccSolisDlg::advancedVisible() const
{
    return advancedToolButton->isChecked();
}

void ccSolisDlg::updateDynamicUi()
{
    const bool instantaneous = temporalModeComboBox->currentIndex() == TemporalInstant;
    const bool integratedEnergy = temporalModeComboBox->currentIndex() == TemporalEnergy;
    const bool visibility = spatialModeComboBox->currentIndex() == SpatialVisibility;
    const bool localTime = timeModeComboBox->currentIndex() == TimeLocal;
    const bool customQuality = qualityPresetComboBox->currentIndex() == QualityCustom;
    const bool direct = calcDirectCheckBox->isChecked();
    const bool diffuse = calcDiffCheckBox->isChecked();

    integrationDoubleSpinBox->setEnabled(!instantaneous);
    durationLabel->setEnabled(!instantaneous);
    timestepDoubleSpinBox->setEnabled(!instantaneous);
    timestepLabel->setEnabled(!instantaneous);

    unitLabel->setEnabled(integratedEnergy && !visibility);
    energyUnitComboBox->setEnabled(integratedEnergy && !visibility);
    if (visibility)
        effectiveUnitLabel->setText(tr("fraction"));
    else if (integratedEnergy)
        effectiveUnitLabel->setText(energyUnitComboBox->currentIndex() == UnitKWh ? tr("kWh/m²") : tr("Wh/m²"));
    else
        effectiveUnitLabel->setText(tr("W/m²"));

    utcOffsetDoubleSpinBox->setEnabled(localTime);
    atmosphereGroupBox->setEnabled(!visibility);

    resSpinBox->setEnabled(customQuality);
    raysSpinBox->setEnabled(customQuality && diffuse);
    rayslabel->setEnabled(customQuality && diffuse);

    const bool canAccelerate = direct && !instantaneous;
    directAccelerationCheckBox->setEnabled(canAccelerate);
    angularToleranceLabel->setEnabled(canAccelerate && directAccelerationCheckBox->isChecked());
    angularToleranceDoubleSpinBox->setEnabled(canAccelerate && directAccelerationCheckBox->isChecked());

    if (visibility && direct && diffuse)
        componentHintLabel->setText(tr("Direct + Diffuse creates two visibility fields; Total is undefined for visibility."));
    else
        componentHintLabel->setText(tr("Direct + Diffuse also creates Total (except in Visibility mode)."));

    qualitySummaryLabel->setText(tr("%1 px / %2 diffuse rays")
                                     .arg(resSpinBox->value())
                                     .arg(raysSpinBox->value()));
}

void ccSolisDlg::updateBatchPreview()
{
    if (!batchGroupBox->isChecked())
    {
        batchCountLabel->setText(tr("1 time point"));
        batchCountLabel->setToolTip(tr("Batch mode is disabled."));
        return;
    }

    const solis::CalculationOptions options = calculationOptions();
    const solis::BatchOptions batch = batchOptions();

    QString error;
    const unsigned count = solis::BatchTimePointCount(options, batch, &error);
    if (count == 0)
    {
        batchCountLabel->setText(tr("Invalid range"));
        batchCountLabel->setToolTip(error);
    }
    else
    {
        batchCountLabel->setText(tr("%1 time point(s)").arg(count));
        batchCountLabel->setToolTip(tr("%1 separate SOLIS result time point(s) will be stored.").arg(count));
    }
}

void ccSolisDlg::applyQualityPreset(int index)
{
    if (index < QualityFast || index > QualityHigh)
        return;

    const QualityPreset preset = QualityPresets[index];
    resSpinBox->setValue(preset.resolution);
    raysSpinBox->setValue(preset.diffuseRays);
}

int ccSolisDlg::presetForCurrentQuality() const
{
    for (int i = QualityFast; i <= QualityHigh; ++i)
    {
        if (resSpinBox->value() == QualityPresets[i].resolution
            && raysSpinBox->value() == QualityPresets[i].diffuseRays)
        {
            return i;
        }
    }
    return QualityCustom;
}
