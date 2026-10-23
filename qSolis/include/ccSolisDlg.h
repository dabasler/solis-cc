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

#ifndef CC_SOLIS_DLG_HEADER
#define CC_SOLIS_DLG_HEADER

#include "SOLIS.h"
#include "SOLISBatch.h"

#include <ui_solisDlg.h>

#include <QString>

//! Dialog for the SOLIS plugin
class ccSolisDlg : public QDialog, public Ui::SOLISDialog
{
public:
    explicit ccSolisDlg(QWidget* parent = nullptr);

    void setCalculationOptions(const solis::CalculationOptions& options);
    solis::CalculationOptions calculationOptions() const;

    void setBatchOptions(const solis::BatchOptions& options);
    solis::BatchOptions batchOptions() const;

    void setComponents(bool direct, bool diffuse);
    bool calculateDirect() const;
    bool calculateDiffuse() const;

    void setSurfaceModeAvailable(bool available, const QString& reason = QString());
    void setHasMeshes(bool hasMeshes);

    void setQualityPresetIndex(int index);
    int qualityPresetIndex() const;

    void setAdvancedVisible(bool visible);
    bool advancedVisible() const;

private:
    void updateDynamicUi();
    void updateBatchPreview();
    void applyQualityPreset(int index);
    int presetForCurrentQuality() const;
};

#endif
