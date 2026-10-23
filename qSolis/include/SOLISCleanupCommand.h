#pragma once

#include <ccCommandLineInterface.h>

//! Removes qSOLIS result scalar fields from loaded entities.
class SOLISCleanupCommand : public ccCommandLineInterface::Command
{
public:
    SOLISCleanupCommand();
    ~SOLISCleanupCommand() override = default;

    bool process(ccCommandLineInterface& cmd) override;
};
