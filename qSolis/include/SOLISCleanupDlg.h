#pragma once

#include <QDialog>
#include <QString>

#include <vector>

class QComboBox;
class QLabel;
class QRadioButton;

namespace solis
{

enum class CleanupChoice
{
    LastRun,
    LastBatch,
    SelectedBatch,
    AllManaged,
    LegacyNamed
};

struct CleanupBatchChoice
{
    QString batchId;
    QString label;
    unsigned resultCount = 0;
};

//! Confirmation/selection dialog for qSOLIS scalar-field cleanup.
class CleanupDialog : public QDialog
{
    Q_OBJECT

public:
    CleanupDialog(unsigned entityCount,
                  unsigned managedResultCount,
                  unsigned legacyResultCount,
                  unsigned lastRunResultCount,
                  unsigned lastBatchResultCount,
                  const std::vector<CleanupBatchChoice>& batches,
                  QWidget* parent = nullptr);

    CleanupChoice choice() const;
    QString selectedBatchId() const;

private:
    void updateSummary();

    unsigned m_managedResultCount = 0;
    unsigned m_legacyResultCount = 0;
    unsigned m_lastRunResultCount = 0;
    unsigned m_lastBatchResultCount = 0;
    std::vector<CleanupBatchChoice> m_batches;

    QRadioButton* m_lastRun = nullptr;
    QRadioButton* m_lastBatch = nullptr;
    QRadioButton* m_selectedBatch = nullptr;
    QRadioButton* m_allManaged = nullptr;
    QRadioButton* m_legacyNamed = nullptr;
    QComboBox* m_batchCombo = nullptr;
    QLabel* m_summary = nullptr;
};

} // namespace solis
