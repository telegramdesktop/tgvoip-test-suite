#include "cmd_args.h"

#include "utils.h"
#include <iostream>

namespace tgvoiprate {

    CmdArgs::CmdArgs(int argc, char** argv)
    {
        try
        {
            throwIf(argc < 3, "not enough arguments");
            initialSoundPath = std::string{argv[1]};
            corruptedSoundPath = std::string{argv[2]};
        }
        catch (const std::runtime_error&)
        {
            PrintUsage();
            throw;
        }
    }

    void CmdArgs::PrintUsage()
    {
            std::cout << "USAGE: tgvoiprate /path/to/sound_A.opus /path/to/sound_output_A.opus" << std::endl;
    }
}
