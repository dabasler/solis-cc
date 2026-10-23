#pragma once

#include "SOLISExposure.h"
#include <ui_solisExposureDlg.h>

class ccSolisExposureDlg : public QDialog, public Ui::SOLISExposureDialog
{
public:
    explicit ccSolisExposureDlg(QWidget* parent = nullptr);
    void setOptions(const solis::ExposureOptions& options);
    solis::ExposureOptions options() const;
    void setSelectionKinds(bool hasPointClouds, bool hasMeshes);
    void setAdvancedVisible(bool visible);
    bool advancedVisible() const;
    void setQualityPresetIndex(int index);
    int qualityPresetIndex() const;

private:
    void updateUi();
    void applyQualityPreset(int index);
    int presetForCurrentQuality() const;

    bool m_hasPointClouds = true;
};
