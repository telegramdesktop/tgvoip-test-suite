#pragma once

#include <string>
#include <array>

namespace tgvoiprate {

struct CmdArgs
{
    CmdArgs(int argc, char** argv);

    std::string initialSoundPath;
    std::string corruptedSoundPath;

private:
    static void PrintUsage();
};

}
