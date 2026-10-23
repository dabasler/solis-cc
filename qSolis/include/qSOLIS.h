#pragma once

#include "ccStdPluginInterface.h"

class qSOLIS : public QObject, public ccStdPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(ccPluginInterface ccStdPluginInterface)
    Q_PLUGIN_METADATA(IID "cccorp.cloudcompare.plugin.qSOLIS" FILE "../info.json")

public:
    explicit qSOLIS(QObject* parent = nullptr);
    ~qSOLIS() override = default;

    void onNewSelection(const ccHObject::Container& selectedEntities) override;
    QList<QAction*> getActions() override;
    void registerCommands(ccCommandLineInterface* cmd) override;

private:
    void doAction();
    void doCleanupAction();
    QAction* m_action = nullptr;
    QAction* m_cleanupAction = nullptr;
};
