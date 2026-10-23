#pragma once

#include <ccCommandLineInterface.h>

//! CloudCompare command-line entry point for qSOLIS.
class SOLISCommand : public ccCommandLineInterface::Command
{
public:
    SOLISCommand();
    ~SOLISCommand() override = default;

    bool process(ccCommandLineInterface& cmd) override;
};
