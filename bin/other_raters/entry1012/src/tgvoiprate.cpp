/*
 *  Daniil Gentili's submission to the VoIP contest.
 *  Copyright (C) 2019 Daniil Gentili <daniil@daniil.it>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "rater.h"

#include <iostream>
#include <string>
#include <opus/opusfile.h>

/*
 * $ tgvoiprate /path/to/sound_A.opus /path/to/sound_output_A.opus
 * 4.6324
 * 
 */
int usage(const char *error, const char *script)
{
    std::cerr << "Daniil Gentili's submission to the VoIP contest  Copyright (C) 2019  Daniil Gentili <daniil@daniil.it>" << std::endl;
    std::cerr << "This program comes with ABSOLUTELY NO WARRANTY; for details see the GPLv3 license." << std::endl;
    std::cerr << "This is free software, and you are welcome to redistribute it under certain conditions." << std::endl;
    std::cerr << std::endl;
    std::cerr << "Usage: " << script << " orig.opus modified.opus [logfile.log]" << std::endl;
    std::cerr << error << std::endl;
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        return usage("", argv[0]);
    }

    try
    {
        Rater rater(argv[0], argv[1], argv[2], argc == 4 ? argv[3] : nullptr);
        std::cout << rater.finalRateWeight() << std::endl;
    }
    catch (std::invalid_argument &exception)
    {
        std::cerr << exception.what() << std::endl;
        return 1;
    }

    return 0;
}
