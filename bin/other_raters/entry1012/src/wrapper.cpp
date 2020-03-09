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
#include <wchar.h>
#include <map>
#include <string>
#include <vector>
#include <queue>
#include <iostream>

#include <opus/opus.h>
#include <opus/opusfile.h>
#include <opus/opusenc.h>

#include "libtgvoip/VoIPServerConfig.h"
#include "libtgvoip/threading.h"

using namespace tgvoip;
using namespace tgvoip::audio;
using namespace std;

bool VoIPWrapper::init(bool creator, std::string ip, int port, std::string tag, std::string key, std::string sharedConfig, std::string inputFile, std::string outputFile, std::string logFile)
{
    // Init decoder
    inputDecoder = op_open_file(inputFile.c_str(), nullptr);
    if (inputDecoder == nullptr)
    {
        wrapperError = "Could not open input file";
        return false;
    }
    // Init opus ogg encoder
    comments = ope_comments_create();
    int error = 0;
    outputEncoder = ope_encoder_create_file(outputFile.c_str(), comments, 48000, 1, 0, &error);
    if (error != OPE_OK)
    {
        wrapperError = std::string("Could not initialize opus encoder: ") + ope_strerror(error);
        return false;
    }
    error = ope_encoder_ctl(outputEncoder, OPUS_SET_BITRATE(64000));
    if (error != OPE_OK)
    {
        wrapperError = std::string("Could not set bitrate of opus encoder: ") + ope_strerror(error);
        return false;
    }
    error = ope_encoder_ctl(outputEncoder, OPUS_SET_VBR(1));
    if (error != OPUS_OK)
    {
        wrapperError = std::string("Could not enable variable bitrate in opus encoder: ") + ope_strerror(error);
        return false;
    }

    initVoIPController();
    ServerConfig::GetSharedInstance()->Update(sharedConfig);
    inst->SetNetworkType(NET_TYPE_WIFI);

    VoIPController::Config cfg;
    cfg.recvTimeout = 3; // CHECK THIS
    cfg.initTimeout = 5;
    cfg.logFilePath = logFile;
    //cfg.dataSaving = (int)self["configuration"]["data_saving"];
    //cfg.enableAEC = (bool)self["configuration"]["enable_AEC"];
    //cfg.enableNS = (bool)self["configuration"]["enable_NS"];
    //cfg.enableAGC = (bool)self["configuration"]["enable_AGC"];
    //cfg.enableCallUpgrade = (bool)self["configuration"]["enable_call_upgrade"];

    /*
    if (self["configuration"]["stats_dump_file_path"])
    {
        std::string statsDumpFilePath = self["configuration"]["stats_dump_file_path"];
        cfg.statsDumpFilePath = statsDumpFilePath;
    }*/
    inst->SetConfig(cfg);

    if (key.length() != 256)
    {
        wrapperError = "Wrong key length";
        return false;
    }
    char *keyC = (char *)malloc(256);
    memcpy(keyC, key.c_str(), 256);
    inst->SetEncryptionKey(keyC, creator);
    free(keyC);

    vector<Endpoint> eps;
    IPv4Address v4addr(ip);
    IPv6Address v6addr("::0");

    // Constant endpoint ID, shouldn't matter anyway
    if (tag.length() != 16)
    {
        wrapperError = "Wrong key length";
        return false;
    }
    unsigned char *pTag = (unsigned char *)malloc(16);
    memcpy(pTag, tag.c_str(), 16);
    eps.push_back(Endpoint(1, port, v4addr, v6addr, Endpoint::Type::UDP_RELAY, pTag));
    free(pTag);

    // Force reflector, max layer is constant as the other end is this script
    inst->SetRemoteEndpoints(eps, false, 92);
    return true;
}
void VoIPWrapper::initVoIPController()
{
    inst = new VoIPController();

    inst->implData = (void *)this;
    VoIPController::Callbacks callbacks;
    callbacks.connectionStateChanged = [](VoIPController *controller, int state) {
        ((VoIPWrapper *)controller->implData)->state = state;
        if (state == STATE_FAILED)
        {
            ((VoIPWrapper *)controller->implData)->finished(false, "libtgvoip exited with state: " + std::to_string(state));
        }
    };
    callbacks.signalBarCountChanged = nullptr;
    callbacks.groupCallKeySent = nullptr;
    callbacks.groupCallKeyReceived = nullptr;
    callbacks.upgradeToGroupCallRequested = nullptr;
    inst->SetCallbacks(callbacks);
    inst->SetAudioDataCallbacks(
        [this](int16_t *buffer, size_t size) {
            this->sendAudioFrame(buffer, size);
        },
        [this](int16_t *buffer, size_t size) {
            this->recvAudioFrame(buffer, size);
        });
}
void VoIPWrapper::recvAudioFrame(int16_t *data, size_t size)
{
    if (outputEncoder != nullptr)
    {
        ope_encoder_write(outputEncoder, data, size);
        wrote = true;
    }
}
void VoIPWrapper::sendAudioFrame(int16_t *data, size_t size)
{
    int res = 0;
    do
    {
        res = op_read(inputDecoder, data, size, NULL);
        if (res < 0)
        {
            finished(false, std::string("Failure reading data: ") + opus_strerror(res));
            return;
        }
        else if (res == 0)
        {
            finished(true);
            return;
        }
        size -= res;
        data += res;
    } while (size);
}

int VoIPWrapper::run()
{
    inst->Start();
    inst->Connect();

    runningSemaphore.Acquire(); // Will be unlocked only if our stream has finished or if an error occurs

    sleep(3);
    deinitVoIPController();

    return wrapperOk;
}

void VoIPWrapper::finished(bool ok, std::string error)
{
    wrapperOk = ok;
    wrapperError = error;

    /*
     * Do not actually deinit controller here, since this function might be called from the audio callbacks 
     * and deiniting all libtgvoip data structures before the callback returns might cause issues.
     */
    runningSemaphore.Release();
}

void VoIPWrapper::deinitVoIPController()
{
    if (inst)
    {
        debugLog = inst->GetDebugLog();
        inst->Stop();
        delete inst;
        inst = nullptr;
    }

    if (inputDecoder)
    {
        op_free(inputDecoder);
        inputDecoder = nullptr;
    }
    if (outputEncoder)
    {
        if (!wrote) {
            // Encode 960 empty samples if nothing was written to prevent exception from ope library
            opus_int16 *buffer = (opus_int16 *)calloc(960, sizeof(opus_int16));
            ope_encoder_write(outputEncoder, buffer, 960);
            free(buffer);
            buffer = nullptr;
        }
        ope_encoder_drain(outputEncoder);
        ope_encoder_destroy(outputEncoder);
        outputEncoder = nullptr;
    }
    if (comments)
    {
        ope_comments_destroy(comments);
        comments = nullptr;
    }
}
