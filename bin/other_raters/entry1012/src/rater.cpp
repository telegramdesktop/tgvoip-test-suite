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

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <exception>

#include <opus/opusfile.h>
#include <opus.h>
#include <pocketsphinx.h>
#include "resampler/speex_resampler.h"

Rater::Rater(const char *me, const char *nameOrig, const char *nameMod, const char *logPath)
{
    // Rater logs will overwrite part of pocketsphinx voice recognition logs, but we didn't need them anyway
    log = std::ofstream(logPath == nullptr ? "/dev/null" : logPath);

    // Open files
    int err = 0;
    fileOrig = op_open_file(nameOrig, &err);
    if (fileOrig == nullptr)
    {
        throw std::invalid_argument("Could not open original file!");
    }
    fileMod = op_open_file(nameMod, &err);
    if (fileMod == nullptr)
    {
        throw std::invalid_argument("Could not open modified file!");
    }

    // Calculate length of files
    lengthOrig = op_pcm_total(fileOrig, -1);
    lengthMod = op_pcm_total(fileMod, -1);

    throwIfOpus("Could not get length of original file: ", lengthOrig);
    throwIfOpus("Could not get length of modified file: ", lengthMod);

    lengthMin = std::min(lengthOrig, lengthMod);

    bufferOrig = (int16_t *)calloc(lengthOrig, sizeof(int16_t));
    bufferMod = (int16_t *)calloc(lengthMod, sizeof(int16_t));

    readToBuffer(fileOrig, bufferOrig, lengthOrig);
    readToBuffer(fileMod, bufferMod, lengthMod);

    std::filesystem::path mePath(me);
    mePath = std::filesystem::canonical(mePath);
    mePath = mePath.parent_path();
    mePath += "/src/assets/model";

    if (!std::filesystem::exists(mePath))
    {
        throw std::invalid_argument(std::string("Voice recognition model path ") + mePath.c_str() + " not found!");
    }
    std::filesystem::path modelPath = mePath;
    std::filesystem::path languagePath = mePath;
    std::filesystem::path dictPath = mePath;

    modelPath += "/en-us-adapt";
    languagePath += "/list.lm";
    dictPath += "/cmudict-en-us.dict";

    if (!std::filesystem::exists(modelPath))
    {
        throw std::invalid_argument(std::string("Voice recognition model path ") + modelPath.c_str() + " not found!");
    }
    if (!std::filesystem::exists(languagePath))
    {
        throw std::invalid_argument(std::string("Voice recognition language model path ") + languagePath.c_str() + " not found!");
    }
    if (!std::filesystem::exists(dictPath))
    {
        throw std::invalid_argument(std::string("Voice recognition dictionary path ") + dictPath.c_str() + " not found!");
    }

    // Init voice recognition
    cmd_ln_t *config = cmd_ln_init(nullptr, ps_args(), TRUE,
                                   "-hmm", modelPath.c_str(),
                                   "-lm", languagePath.c_str(),
                                   "-dict", dictPath.c_str(),
                                   "-logfn", logPath == nullptr ? "/dev/null" : logPath,
                                   nullptr);
    ps = ps_init(config);
}

Rater::~Rater()
{
    free(bufferOrig);
    free(bufferMod);
    bufferOrig = nullptr;
    bufferMod = nullptr;
    op_free(fileOrig);
    op_free(fileMod);
    fileOrig = nullptr;
    fileMod = nullptr;

    speex_resampler_destroy(state);
    state = nullptr;

    free(resampleBuffer16);
    resampleBuffer16 = nullptr;

    ps_free(ps);
    ps = nullptr;

    log.close();
}

bool Rater::readToBuffer(OggOpusFile *file, int16_t *buffer, size_t size)
{
    int res = 0;
    do
    {
        res = op_read(file, buffer, size, NULL);
        throwIfOpus("Failure reading data!", res);
        if (res == 0)
        {
            throw std::invalid_argument("Read no data!");
        }

        size -= res;
        buffer += res;
    } while (size);
    return true;
}
double Rater::rateLength()
{
    double rating = 5.0;
    if (lengthMod < lengthOrig)
    {
        rating = (lengthMod * 5.0) / lengthOrig;
    }
    return rating;
}
double Rater::rateSilence()
{
    uint64_t silenceOrig = 0;
    uint64_t silenceMod = 0;
    uint64_t curSilenceOrig = 0;
    uint64_t curSilenceMod = 0;
    for (size_t x = 0; x < lengthMin; x++)
    {
        if (std::abs(bufferOrig[x]) < SILENCE_THRESHOLD)
        {
            curSilenceOrig++;
        }
        else
        {
            if (curSilenceOrig > MIN_SILENCE_SAMPLES)
            {
                silenceOrig += curSilenceOrig;
            }
            curSilenceOrig = 0;
        }
        if (std::abs(bufferMod[x]) < SILENCE_THRESHOLD)
        {
            curSilenceMod++;
        }
        else
        {
            if (curSilenceMod > MIN_SILENCE_SAMPLES)
            {
                silenceMod += curSilenceMod;
            }
            curSilenceMod = 0;
        }
    }
    if (curSilenceOrig > MIN_SILENCE_SAMPLES)
    {
        silenceOrig += curSilenceOrig;
    }

    if (curSilenceMod > MIN_SILENCE_SAMPLES)
    {
        silenceMod += curSilenceMod;
    }

    // The code above basically counts the number of consecutive samples with near-silence (due to glitches)
    // Then we compare it to the number of silence samples in the original file
    double rating = 5.0;
    if (silenceMod > silenceOrig)
    {
        rating = 5.0 - (((silenceMod - silenceOrig) * 5.0) / (lengthMin - silenceOrig));
    }
    log << silenceOrig << " - " << silenceMod << " total " << lengthMin << std::endl;
    return rating;
}
double Rater::rateVoiceRecognition()
{
    int32 scoreOrig, scoreMod;
    std::string recogOrig, recogMod;

    // First a dry run to warm up voice recognition
    recogOrig = voiceRecognition(bufferOrig, lengthOrig, &scoreOrig);
    log << "Recognized original (warmup, " << logmath_exp(ps_get_logmath(ps), scoreOrig) << ") " << recogOrig << std::endl;

    // Then recognize original buffer
    recogOrig = voiceRecognition(bufferOrig, lengthOrig, &scoreOrig);
    // Then recognize new buffer
    recogMod = voiceRecognition(bufferMod, lengthMod, &scoreMod);

    log << "Recognized original (" << logmath_exp(ps_get_logmath(ps), scoreOrig) << ")         " << recogOrig << std::endl;
    log << "Recognized modified (" << logmath_exp(ps_get_logmath(ps), scoreMod) << ")         " << recogMod << std::endl;

    // Don't use the score to generate the rating, it's more reliable to directly compare the generated strings
    if (recogOrig == recogMod)
    {
        return 5.0;
    }
    return (php_similar_char(recogOrig.c_str(), recogOrig.length(), recogMod.c_str(), recogMod.length()) * 5.0) / recogOrig.length();
}

std::string Rater::voiceRecognition(int16_t *buffer, size_t length, int32 *score)
{
    ps_start_utt(ps);

    for (size_t x = 0; x < length; x += RESAMPLE_SIZE48)
    {
        resample(buffer + x, resampleBuffer16);
        ps_process_raw(ps, resampleBuffer16, RESAMPLE_SIZE16, FALSE, FALSE);
    }
    ps_end_utt(ps);

    return std::string(ps_get_hyp(ps, score));
}

double Rater::finalRateWeight()
{
    double ratingLength = rateLength();
    double ratingSilence = rateSilence();
    double ratingVoiceRecognition = rateVoiceRecognition();

    log << "Length rating: " << ratingLength << std::endl;
    log << "Silence rating: " << ratingSilence << std::endl;
    log << "Voice recognition rating: " << ratingVoiceRecognition << std::endl;

    double final = ratingVoiceRecognition;
    if (ratingSilence < 5.0)
    {
        // 60% voice recognition, 40% silence recognition
        final = ((final * 60.0) + (ratingSilence * 40.00)) / 100.0;
    }
    if (ratingLength < 5.0)
    {
        // 40% voice+silence recognition, 60% silence recognition
        final = ((final * 40.0) + (ratingLength * 60.00)) / 100.0;
    }

    // If more than half of the words are missing/different, round down to account for human lack of patience
    if (ratingVoiceRecognition < 2.5)
    {
        final = std::max(final - 1.25, 0.0);
    }

    return final;
}
void Rater::resample(int16_t *in, int16_t *out)
{
    uint32_t in_len = RESAMPLE_SIZE48;
    uint32_t out_len = RESAMPLE_SIZE16;
    speex_resampler_process_int(state, 0, in, &in_len, out, &out_len);
}
