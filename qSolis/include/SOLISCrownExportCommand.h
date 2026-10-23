#pragma once

#include <ccCommandLineInterface.h>

//! Exports persisted qSOLIS Crown Exposure summaries to CSV.
class SOLISCrownExportCommand : public ccCommandLineInterface::Command
{
public:
    SOLISCrownExportCommand();
    ~SOLISCrownExportCommand() override = default;

    bool process(ccCommandLineInterface& cmd) override;
};
