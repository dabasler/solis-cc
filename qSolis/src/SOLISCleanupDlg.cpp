#include "SOLISCleanupDlg.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace solis
{

CleanupDialog::CleanupDialog(unsigned entityCount,
                             unsigned managedResultCount,
                             unsigned legacyResultCount,
                             unsigned lastRunResultCount,
                             unsigned lastBatchResultCount,
                             const std::vector<CleanupBatchChoice>& batches,
                             QWidget* parent)
    : QDialog(parent)
    , m_managedResultCount(managedResultCount)
    , m_legacyResultCount(legacyResultCount)
    , m_lastRunResultCount(lastRunResultCount)
    , m_lastBatchResultCount(lastBatchResultCount)
    , m_batches(batches)
{
    setWindowTitle(tr("SOLIS Cleanup"));
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(
        tr("Remove qSOLIS result scalar fields from %1 selected eligible %2. "
           "Metadata-backed cleanup never removes unrelated scalar fields.")
            .arg(entityCount)
            .arg(entityCount == 1 ? tr("entity") : tr("entities")),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_lastRun = new QRadioButton(tr("Remove last SOLIS run (%1 fields)").arg(lastRunResultCount), this);
    m_lastBatch = new QRadioButton(tr("Remove last SOLIS batch (%1 fields)").arg(lastBatchResultCount), this);
    m_selectedBatch = new QRadioButton(tr("Remove selected SOLIS batch"), this);
    m_allManaged = new QRadioButton(tr("Remove all metadata-backed SOLIS results (%1 fields)").arg(managedResultCount), this);
    m_legacyNamed = new QRadioButton(
        tr("Remove legacy SOLIS-named fields without metadata (%1 fields) — name match only").arg(legacyResultCount),
        this);

    m_lastRun->setEnabled(lastRunResultCount != 0);
    m_lastBatch->setEnabled(lastBatchResultCount != 0);
    m_selectedBatch->setEnabled(!batches.empty());
    m_allManaged->setEnabled(managedResultCount != 0);
    m_legacyNamed->setEnabled(legacyResultCount != 0);

    layout->addWidget(m_lastRun);
    layout->addWidget(m_lastBatch);
    layout->addWidget(m_selectedBatch);

    m_batchCombo = new QComboBox(this);
    for (const CleanupBatchChoice& batch : batches)
    {
        m_batchCombo->addItem(batch.label, batch.batchId);
    }
    m_batchCombo->setEnabled(!batches.empty());
    layout->addWidget(m_batchCombo);
    layout->addWidget(m_allManaged);
    layout->addWidget(m_legacyNamed);

    auto* warning = new QLabel(
        tr("Legacy cleanup is intentionally separate because it identifies fields only by the 'SOLIS ' name prefix. "
           "Deletion cannot be undone after the project is saved."),
        this);
    warning->setWordWrap(true);
    layout->addWidget(warning);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Remove"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    const auto connectRadio = [this](QRadioButton* button)
    {
        connect(button, &QRadioButton::toggled, this, [this](bool) { updateSummary(); });
    };
    connectRadio(m_lastRun);
    connectRadio(m_lastBatch);
    connectRadio(m_selectedBatch);
    connectRadio(m_allManaged);
    connectRadio(m_legacyNamed);
    connect(m_batchCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updateSummary(); });

    if (m_lastBatch->isEnabled())
        m_lastBatch->setChecked(true);
    else if (m_lastRun->isEnabled())
        m_lastRun->setChecked(true);
    else if (m_allManaged->isEnabled())
        m_allManaged->setChecked(true);
    else if (m_legacyNamed->isEnabled())
        m_legacyNamed->setChecked(true);

    buttons->button(QDialogButtonBox::Ok)->setEnabled(
        m_managedResultCount != 0 || m_legacyResultCount != 0);
    updateSummary();
}

CleanupChoice CleanupDialog::choice() const
{
    if (m_lastRun->isChecked())
        return CleanupChoice::LastRun;
    if (m_lastBatch->isChecked())
        return CleanupChoice::LastBatch;
    if (m_selectedBatch->isChecked())
        return CleanupChoice::SelectedBatch;
    if (m_legacyNamed->isChecked())
        return CleanupChoice::LegacyNamed;
    return CleanupChoice::AllManaged;
}

QString CleanupDialog::selectedBatchId() const
{
    return m_batchCombo->currentData().toString();
}

void CleanupDialog::updateSummary()
{
    unsigned count = 0;
    QString description;

    switch (choice())
    {
    case CleanupChoice::LastRun:
        count = m_lastRunResultCount;
        description = tr("last run");
        break;
    case CleanupChoice::LastBatch:
        count = m_lastBatchResultCount;
        description = tr("last batch");
        break;
    case CleanupChoice::SelectedBatch:
        description = tr("selected batch");
        if (m_batchCombo->currentIndex() >= 0)
        {
            const QString id = selectedBatchId();
            for (const CleanupBatchChoice& batch : m_batches)
            {
                if (batch.batchId == id)
                {
                    count = batch.resultCount;
                    break;
                }
            }
        }
        break;
    case CleanupChoice::AllManaged:
        count = m_managedResultCount;
        description = tr("all metadata-backed results");
        break;
    case CleanupChoice::LegacyNamed:
        count = m_legacyResultCount;
        description = tr("legacy SOLIS-named fields");
        break;
    }

    m_batchCombo->setEnabled(m_selectedBatch->isChecked() && !m_batches.empty());
    m_summary->setText(tr("This will remove approximately %1 scalar field(s) from the %2 selection.")
                           .arg(count)
                           .arg(description));
}

} // namespace solis
