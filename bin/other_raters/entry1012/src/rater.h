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

#ifndef RATER_H
#define RATER_H

#ifndef TGVOIP_USE_DESKTOP_DSP
#define TGVOIP_USE_DESKTOP_DSP
#endif

#define MIN_SILENCE_MS 100
#define MIN_SILENCE_SAMPLES MIN_SILENCE_MS*48

#define SILENCE_THRESHOLD 3276 // 32768/10

// 2048 samples for each voice recognition pass at 16khz (128ms)
#define RESAMPLE_SIZE16 2048
// 6144 samples for each voice recognition pass at 48khz (128ms)
#define RESAMPLE_SIZE48 (RESAMPLE_SIZE16/16)*48

#include <exception>
#include <iostream>
#include <fstream>

#include <opus/opusfile.h>
#include <pocketsphinx.h>
#include "resampler/speex_resampler.h"

class Rater
{
public:
    Rater(const char *me, const char *nameOrig, const char *nameMod, const char *logPath = nullptr);
    ~Rater();

    double rateLength();
    double rateSilence();
    double rateVoiceRecognition();

    double finalRateWeight();

    std::ofstream log;
private:
    std::string voiceRecognition(int16_t *buffer, size_t length, int32 *score);
    void resample(int16_t *in, int16_t *out);
    bool readToBuffer(OggOpusFile *file, int16_t *buffer, size_t size);
    void throwIfOpus(const char *ctx, int err) {
        if (err < 0) {
            throw std::invalid_argument(std::string(ctx) + opus_strerror(err));
        }
    }

    ps_decoder_t *ps = NULL;
    OggOpusFile *fileOrig = nullptr;
    OggOpusFile *fileMod = nullptr;

    int16_t *bufferOrig = nullptr;
    int16_t *bufferMod = nullptr;

    ogg_int64_t lengthOrig = 0;
    ogg_int64_t lengthMod = 0;
    size_t lengthMin = 0;

    SpeexResamplerState *state = speex_resampler_init(1, 48000, 16000, 10, NULL);

    int16_t *resampleBuffer16 = (int16_t *) calloc(RESAMPLE_SIZE16, sizeof(int16_t));
};


/* {{{ php_similar_str
 */
static void php_similar_str(const char *txt1, size_t len1, const char *txt2, size_t len2, size_t *pos1, size_t *pos2, size_t *max, size_t *count)
{
    const char *p, *q;
    const char *end1 = (char *)txt1 + len1;
    const char *end2 = (char *)txt2 + len2;
    size_t l;

    *max = 0;
    *count = 0;
    for (p = (char *)txt1; p < end1; p++)
    {
        for (q = (char *)txt2; q < end2; q++)
        {
            for (l = 0; (p + l < end1) && (q + l < end2) && (p[l] == q[l]); l++)
                ;
            if (l > *max)
            {
                *max = l;
                *count += 1;
                *pos1 = p - txt1;
                *pos2 = q - txt2;
            }
        }
    }
}
/* }}} */

/* {{{ php_similar_char
 */
static size_t php_similar_char(const char *txt1, size_t len1, const char *txt2, size_t len2)
{
    size_t sum;
    size_t pos1 = 0, pos2 = 0, max, count;

    php_similar_str(txt1, len1, txt2, len2, &pos1, &pos2, &max, &count);
    if ((sum = max))
    {
        if (pos1 && pos2 && count > 1)
        {
            sum += php_similar_char(txt1, pos1,
                                    txt2, pos2);
        }
        if ((pos1 + max < len1) && (pos2 + max < len2))
        {
            sum += php_similar_char(txt1 + pos1 + max, len1 - pos1 - max,
                                    txt2 + pos2 + max, len2 - pos2 - max);
        }
    }

    return sum;
}
#endif // RATER_H
