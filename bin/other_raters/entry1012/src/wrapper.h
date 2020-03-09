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


#ifndef PHPLIBTGVOIP_H
#define PHPLIBTGVOIP_H

#ifndef TGVOIP_USE_CALLBACK_AUDIO_IO
#define TGVOIP_USE_CALLBACK_AUDIO_IO
#endif

#ifndef WEBRTC_LINUX
#define WEBRTC_LINUX
#endif

#ifndef WEBRTC_POSIX
#define WEBRTC_POSIX
#endif

#ifndef TGVOIP_USE_DESKTOP_DSP
#define TGVOIP_USE_DESKTOP_DSP
#endif

#ifndef WEBRTC_APM_DEBUG_DUMP
#define WEBRTC_APM_DEBUG_DUMP 0
#endif

#ifndef WEBRTC_NS_FLOAT
#define WEBRTC_NS_FLOAT
#endif

#include <stdio.h>

#include <opus/opusfile.h>
#include <opus/opusenc.h>

#include "libtgvoip/VoIPController.h"
#include "libtgvoip/threading.h"


using namespace tgvoip;
using namespace tgvoip::audio;

namespace tgvoip
{
namespace audio
{
class AudioInputModule;
class AudioOutputModule;
}
}

class VoIPWrapper
{
public:
    bool init(bool creator, std::string ip, int port, std::string tag, std::string key, std::string config, std::string inputFile, std::string outputFile, std::string logFile = "");

    int run();
    void finished(bool ok, std::string error = "");

    void recvAudioFrame(int16_t* data, size_t size);
    void sendAudioFrame(int16_t* data, size_t size);

    std::string getDebugLog() {
        return debugLog;
    }
    std::string getWrapperError() {
        return wrapperError;
    }
    int getState() {
        return state;
    }
    int state = 0;

private:
    void initVoIPController();
    void deinitVoIPController();

    OggOpusFile *inputDecoder = nullptr;
    OggOpusEnc *outputEncoder = nullptr;
    OggOpusComments *comments = nullptr;

    Semaphore runningSemaphore = Semaphore(1, 0);

    bool wrote = false;
    VoIPController *inst = nullptr;


    std::string wrapperError = "";
    bool wrapperOk = true;
    std::string debugLog = "";
};

#endif
