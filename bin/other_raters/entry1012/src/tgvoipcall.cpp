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

#include "wrapper.h"

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>

#include "libtgvoip/VoIPController.h"
#include "libtgvoip/VoIPServerConfig.h"
#include "libtgvoip/threading.h"
#include "libtgvoip/logging.h"

using namespace tgvoip;
using namespace tgvoip::audio;
using namespace std;

std::string hex2bin(std::string const &s)
{
    std::string sOut;
    sOut.reserve(s.length() / 2);

    std::string extract;
    for (std::string::const_iterator pos = s.begin(); pos < s.end(); pos += 2)
    {
        extract.assign(pos, pos + 2);
        sOut.push_back(std::stoi(extract, nullptr, 16));
    }
    return sOut;
}

/*
 * tgvoipcall reflector:port tag_caller_hex -k encryption_key_hex -i /path/to/sound_A.opus -o /path/to/sound_output_B.opus -c config.json -r caller
 * tgvoipcall reflector:port tag_callee_hex -k encryption_key_hex -i /path/to/sound_B.opus -o /path/to/sound_output_A.opus -c config.json -r callee
 *
 * e.g.
 * $ tgvoipcall 134.209.176.124:553 fedd4f39e89991f1d7b1 -k 61362f271d0e13c59...c5eb9ba9ca9daa06c9f -i sound_B.opus -o sound_output_A.opus -c config.json -r caller
 * {"libtgvoip_version":"2.4.4","log_type":"call_stats","network":{"type":"wifi"},"p2p_type":"inet","packet_stats":{"in":1049,"lost_in":4,"lost_out":0,"out":1042},"pref_relay":"2243506735106","problems":[],"protocol_version":9,"relay_rtt":87,"rtt":5,"tcp_used":false,"udp_avail":false}    
 *
 */
int usage(const char *error, const char *script)
{
    std::cerr << "Daniil Gentili's submission to the VoIP contest  Copyright (C) 2019  Daniil Gentili <daniil@daniil.it>" << std::endl;
    std::cerr << "This program comes with ABSOLUTELY NO WARRANTY; for details see the GPLv3 license." << std::endl;
    std::cerr << "This is free software, and you are welcome to redistribute it under certain conditions." << std::endl;
    std::cerr << std::endl;
    std::cerr << "Usage: " << script << " reflector:port tag_caller_hex -k encryption_key_hex -i /path/to/sound_A.opus -o /path/to/sound_output_B.opus -c config.json -r caller/callee [logfile.log]" << std::endl;
    std::cerr << error << std::endl;
    return 1;
}
int main(int argc, char **argv)
{
    if (argc < 13)
    {
        usage("", argv[0]);
        return 1;
    }
    char *ipPort = argv[1];
    char *portC = strstr(ipPort, ":");
    if (portC == nullptr || portC == 0 || strlen(portC) < 2)
    {
        return usage("Bad IP:port!", argv[0]);
    }
    int port = stoi(portC + 1);
    portC[0] = '\0';

    std::string ip = ipPort;

    if (strlen(argv[2]) != 32)
    {
        return usage("Wrong reflector tag length!", argv[0]);
    }
    std::string tag = hex2bin(argv[2]);
    std::string logFile = "";
    if (argc == 14)
    {
        logFile = argv[13];
    }
    std::string key;
    std::string inputFile;
    std::string outputFile;
    std::string configFile;
    bool creator = true;

    optind = 3;
    int opt;
    while ((opt = getopt(argc, argv, "k:i:o:c:r:")) != -1)
    {
        switch (opt)
        {
        case 'k':
            if (strlen(optarg) != 512)
            {
                return usage("Wrong key length!", argv[0]);
            }
            key = hex2bin(optarg);
            break;
        case 'i':
            inputFile = optarg;
            break;
        case 'o':
            outputFile = optarg;
            break;
        case 'c':
            configFile = optarg;
            break;
        case 'r':
            if (strcmp(optarg, "caller") == 0)
            {
                creator = true;
            }
            else if (strcmp(optarg, "callee") == 0)
            {
                creator = false;
            }
            else
            {
                return usage("Must be either caller or callee", argv[0]);
            }
            break;
        default:
            std::cerr << "Invalid argument: " << opt << std::endl;
            return usage("", argv[0]);
        }
    }
    std::ifstream tmp(configFile);
    std::stringstream buffer;
    buffer << tmp.rdbuf();
    std::string config = buffer.str();
    tmp.close();

    VoIPWrapper *wrapper = new VoIPWrapper;
    if (!wrapper->init(creator, ip, port, tag, key, config, inputFile, outputFile, logFile))
    {
        std::cerr << wrapper->getWrapperError() << std::endl;
        return 1;
    }
    if (!wrapper->run())
    {
        std::cerr << wrapper->getWrapperError() << std::endl;
        std::cout << wrapper->getDebugLog() << std::endl;
        return 1;
    }
    std::cout << wrapper->getDebugLog() << std::endl;
    return 0;
}
