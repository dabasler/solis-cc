#pragma once
#include <ccCommandLineInterface.h>

class SOLISExposureCommand : public ccCommandLineInterface::Command
{
public:
    SOLISExposureCommand();
    ~SOLISExposureCommand() override = default;
    bool process(ccCommandLineInterface& cmd) override;
};
