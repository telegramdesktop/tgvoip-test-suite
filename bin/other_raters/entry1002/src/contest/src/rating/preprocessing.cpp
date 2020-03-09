#include <cmath>
#include <cstring>
#include <algorithm>

#include "processing.hpp"
#include "dsp.hpp"


namespace tgvoipcontest {

void dc_block(Signal data, long size) {
    float* p;
    long count;
    float facc = 0.0f;

    long ofs = magic::SEARCHBUFFER * magic::DOWNSAMPLE;

    p = data.begin() + ofs;
    for (count = (size - 2 * ofs); count > 0L; count--)
        facc += *(p++);
    facc /= size;

    p = data.begin() + ofs;
    for (count = (size - 2 * ofs); count > 0L; count--)
        *(p++) -= facc;

    p = data.begin() + ofs;
    for (count = 0L; count < magic::DOWNSAMPLE; count++)
        *(p++) *= (0.5f + count) / magic::DOWNSAMPLE;

    p = data.begin() + size - ofs - 1L;
    for (count = 0L; count < magic::DOWNSAMPLE; count++)
        *(p--) *= (0.5f + count) / magic::DOWNSAMPLE;
}

void apply_filters(Signal data) {
    dsp::iir_filter(magic::InIIR_Hsos, magic::InIIR_Nsos, data.begin(), data.size());
}

float interpolate(float freq,
                  double filter_curve_db[][2],
                  int number_of_points) {
    double result;
    int i;
    double freqLow, freqHigh;
    double curveLow, curveHigh;

    if (freq <= filter_curve_db[0][0]) {
        freqLow = filter_curve_db[0][0];
        curveLow = filter_curve_db[0][1];
        freqHigh = filter_curve_db[1][0];
        curveHigh = filter_curve_db[1][1];

        result = ((freq - freqLow) * curveHigh + (freqHigh - freq) * curveLow) / (freqHigh - freqLow);

        return (float) result;
    }

    if (freq >= filter_curve_db[number_of_points - 1][0]) {
        freqLow = filter_curve_db[number_of_points - 2][0];
        curveLow = filter_curve_db[number_of_points - 2][1];
        freqHigh = filter_curve_db[number_of_points - 1][0];
        curveHigh = filter_curve_db[number_of_points - 1][1];

        result = ((freq - freqLow) * curveHigh + (freqHigh - freq) * curveLow) / (freqHigh - freqLow);

        return (float) result;
    }

    i = 1;
    freqHigh = filter_curve_db[i][0];
    while (freqHigh < freq) {
        i++;
        freqHigh = filter_curve_db[i][0];
    }
    curveHigh = filter_curve_db[i][1];

    freqLow = filter_curve_db[i - 1][0];
    curveLow = filter_curve_db[i - 1][1];

    result = ((freq - freqLow) * curveHigh + (freqHigh - freq) * curveLow) / (freqHigh - freqLow);

    return (float) result;
}


void apply_filter(Signal data, int number_of_points, double filter_curve_db[][2]) {
    long n = data.size() - 2 * magic::SEARCHBUFFER * magic::DOWNSAMPLE;
    long pow_of_2 = dsp::nextpow2(n);
    auto x = Signal(pow_of_2 + 2);

    float overallGainFilter = interpolate(1000.0f, filter_curve_db, number_of_points);

    std::copy_n(data.begin() + magic::SEARCHBUFFER * magic::DOWNSAMPLE, n, x.begin());

    dsp::FFTContext fft_ctx;
    fft_ctx.real_fwd(x.begin(), pow_of_2);

    float freq_resolution = (float) magic::SAMPLE_RATE / (float) pow_of_2;

    float factorDb, factor;
    for (long i = 0; i <= pow_of_2 / 2; i++) {
        factorDb = interpolate(i * freq_resolution, filter_curve_db, number_of_points) - overallGainFilter;
        factor = std::pow(10.0f, factorDb / 20.0f);

        x[2 * i] *= factor;
        x[2 * i + 1] *= factor;
    }

    fft_ctx.real_inv(x.begin(), pow_of_2);
    std::copy_n(x.begin(), n, data.begin() + magic::SEARCHBUFFER * magic::DOWNSAMPLE);
}

void apply_VAD(long n_samples, const Signal data, Signal VAD, Signal logVAD) {
    float g;
    float LevelThresh;
    float LevelNoise;
    float StDNoise;
    float LevelSig;
    float LevelMin;
    long length;
    long n_windows = n_samples / magic::DOWNSAMPLE;

    for (long count = 0L; count < n_windows; count++) {
        VAD[count] = 0.0f;
        for (long iteration = 0L; iteration < magic::DOWNSAMPLE; iteration++) {
            g = data[count * magic::DOWNSAMPLE + iteration];
            VAD[count] += (g * g);
        }
        VAD[count] /= magic::DOWNSAMPLE;
    }

    LevelThresh = 0.0f;
    for (long count = 0L; count < n_windows; count++)
        LevelThresh += VAD[count];
    LevelThresh /= n_windows;

    LevelMin = 0.0f;
    for (long count = 0L; count < n_windows; count++)
        if (VAD[count] > LevelMin)
            LevelMin = VAD[count];
    if (LevelMin > 0.0f)
        LevelMin *= 1.0e-4f;
    else
        LevelMin = 1.0f;

    for (long count = 0L; count < n_windows; count++)
        if (VAD[count] < LevelMin)
            VAD[count] = LevelMin;

    for (long iteration = 0L; iteration < 12L; iteration++) {
        LevelNoise = 0.0f;
        StDNoise = 0.0f;
        length = 0L;
        for (long count = 0L; count < n_windows; count++)
            if (VAD[count] <= LevelThresh) {
                LevelNoise += VAD[count];
                length++;
            }
        if (length > 0L) {
            LevelNoise /= length;
            for (long count = 0L; count < n_windows; count++)
                if (VAD[count] <= LevelThresh) {
                    g = VAD[count] - LevelNoise;
                    StDNoise += g * g;
                }
            StDNoise = (float) std::sqrt(StDNoise / length);
        }

        LevelThresh = 1.001f * (LevelNoise + 2.0f * StDNoise);
    }

    LevelNoise = 0.0f;
    LevelSig = 0.0f;
    length = 0L;
    for (long count = 0L; count < n_windows; count++) {
        if (VAD[count] > LevelThresh) {
            LevelSig += VAD[count];
            length++;
        } else
            LevelNoise += VAD[count];
    }
    if (length > 0L)
        LevelSig /= length;
    else
        LevelThresh = -1.0f;
    if (length < n_windows)
        LevelNoise /= (n_windows - length);
    else
        LevelNoise = 1.0f;

    for (long count = 0L; count < n_windows; count++)
        if (VAD[count] <= LevelThresh)
            VAD[count] = -VAD[count];

    VAD[0] = -LevelMin;
    VAD[n_windows - 1] = -LevelMin;

    long start = 0L;
    for (long count = 1; count < n_windows; count++) {
        if ((VAD[count] > 0.0f) && (VAD[count - 1] <= 0.0f))
            start = count;
        if ((VAD[count] <= 0.0f) && (VAD[count - 1] > 0.0f)) {
            long finish = count;
            if ((finish - start) <= magic::MIN_SPEECH_LEN)
                for (long iteration = start; iteration < finish; iteration++)
                    VAD[iteration] = -VAD[iteration];
        }
    }

    if (LevelSig >= (LevelNoise * 1000.0f)) {
        for (long count = 1; count < n_windows; count++) {
            if ((VAD[count] > 0.0f) && (VAD[count - 1] <= 0.0f))
                start = count;
            if ((VAD[count] <= 0.0f) && (VAD[count - 1] > 0.0f)) {
                long finish = count;
                g = 0.0f;
                for (long iteration = start; iteration < finish; iteration++)
                    g += VAD[iteration];
                if (g < 3.0f * LevelThresh * (finish - start))
                    for (long iteration = start; iteration < finish; iteration++)
                        VAD[iteration] = -VAD[iteration];
            }
        }
    }

    {
        long finish = 0L;
        for (long count = 1; count < n_windows; count++) {
            if ((VAD[count] > 0.0f) && (VAD[count - 1] <= 0.0f)) {
                start = count;
                if ((finish > 0L) && ((start - finish) <= magic::JOIN_SPEECH_LEN))
                    for (long iteration = finish; iteration < start; iteration++)
                        VAD[iteration] = LevelMin;
            }
            if ((VAD[count] <= 0.0f) && (VAD[count - 1] > 0.0f))
                finish = count;
        }

        start = 0L;
        for (long count = 1; count < n_windows; count++) {
            if ((VAD[count] > 0.0f) && (VAD[count - 1] <= 0.0f))
                start = count;
        }
        if (start == 0L) {
            for (long count = 0L; count < n_windows; count++)
                VAD[count] = (float) std::abs(VAD[count]);
            VAD[0] = -LevelMin;
            VAD[n_windows - 1] = -LevelMin;
        }

        long count = 3;
        while (count < (n_windows - 2)) {
            if ((VAD[count] > 0.0f) && (VAD[count - 2] <= 0.0f)) {
                VAD[count - 2] = VAD[count] * 0.1f;
                VAD[count - 1] = VAD[count] * 0.3f;
                count++;
            }
            if ((VAD[count] <= 0.0f) && (VAD[count - 1] > 0.0f)) {
                VAD[count] = VAD[count - 1] * 0.3f;
                VAD[count + 1] = VAD[count - 1] * 0.1f;
                count += 3;
            }
            count++;
        }
    }

    for (long count = 0L; count < n_windows; count++)
        if (VAD[count] < 0.0f) VAD[count] = 0.0f;

    if (LevelThresh <= 0.0f)
        LevelThresh = LevelMin;
    for (long count = 0L; count < n_windows; count++) {
        if (VAD[count] <= LevelThresh)
            logVAD[count] = 0.0f;
        else
            logVAD[count] = (float) std::log(VAD[count] / LevelThresh);
    }
}

void crude_align(RatingContext& info, long piece_id, Signal ftmp) {
    long nr;
    long nd;
    long startr;
    long startd;
    long count;
    Series ref_VAD = info.src.logVAD;
    Series deg_VAD = info.rec.logVAD;

    if (piece_id == magic::WHOLE_SIGNAL) {
        nr = info.src.n_samples / magic::DOWNSAMPLE;
        nd = info.rec.n_samples / magic::DOWNSAMPLE;
        startr = 0L;
        startd = 0L;
    } else if (piece_id == magic::MAX_PIECES) {
        startr = info.piece_search_start[magic::MAX_PIECES - 1];
        startd = startr + info.piece_delay_est[magic::MAX_PIECES - 1] / magic::DOWNSAMPLE;

        if (startd < 0L) {
            startr = -info.piece_delay_est[magic::MAX_PIECES - 1] / magic::DOWNSAMPLE;
            startd = 0L;
        }

        nr = info.piece_search_end[magic::MAX_PIECES - 1] - startr;
        nd = nr;

        if (startd + nd > info.rec.n_samples / magic::DOWNSAMPLE)
            nd = info.rec.n_samples / magic::DOWNSAMPLE - startd;
    } else {
        startr = info.piece_search_start[piece_id];
        startd = startr + info.crude_delay / magic::DOWNSAMPLE;

        if (startd < 0L) {
            startr = -info.crude_delay / magic::DOWNSAMPLE;
            startd = 0L;
        }

        nr = info.piece_search_end[piece_id] - startr;
        nd = nr;

        if (startd + nd > info.rec.n_samples / magic::DOWNSAMPLE)
            nd = info.rec.n_samples / magic::DOWNSAMPLE - startd;
    }

    auto Y = ftmp;
    if ((nr > 1L) && (nd > 1L))
        dsp::FFTContext{}.correlations(ref_VAD.begin() + startr, nr, deg_VAD.begin() + startd, nd, Y.begin());

    float max = 0.0f;
    long I_max = nr - 1;
    if ((nr > 1L) && (nd > 1L))
        for (count = 0L; count < (nr + nd - 1); count++)
            if (Y[count] > max) {
                max = Y[count];
                I_max = count;
            }

    if (piece_id == magic::WHOLE_SIGNAL) {
        info.crude_delay = (I_max - nr + 1) * magic::DOWNSAMPLE;
    } else if (piece_id == magic::MAX_PIECES) {
        info.piece_delay[magic::MAX_PIECES - 1] =
            (I_max - nr + 1) * magic::DOWNSAMPLE + info.piece_delay_est[magic::MAX_PIECES - 1];
    } else {
        info.piece_delay_est[piece_id] =
            (I_max - nr + 1) * magic::DOWNSAMPLE + info.crude_delay;
    }
}

void time_align(RatingContext& info, long piece_id, Signal ftmp) {
    long I_max;
    float v_max;
    long estdelay;
    long startr;
    long startd;
    float* window;
    float r1, i1;
    long kernel;
    float Hsum;

    estdelay = info.piece_delay_est[piece_id];

    float* X1 = ftmp.begin();
    float* X2 = ftmp.begin() + magic::Align_Nfft + 2;
    float* H = (ftmp.begin() + 4 + 2 * magic::Align_Nfft);
    for (long count = 0L; count < magic::Align_Nfft; count++)
        H[count] = 0.0f;
    window = ftmp.begin() + 5 * magic::Align_Nfft;

    for (long count = 0L; count < magic::Align_Nfft; count++)
        window[count] = (float) (0.5 * (1.0 - std::cos((magic::TWOPI * count) / magic::Align_Nfft)));

    startr = info.piece_search_start[piece_id] * magic::DOWNSAMPLE;
    startd = startr + estdelay;

    if (startd < 0L) {
        startr = -estdelay;
        startd = 0L;
    }

    dsp::FFTContext fft_ctx;
    while (((startd + magic::Align_Nfft) <= info.rec.n_samples) &&
           ((startr + magic::Align_Nfft) <= (info.piece_search_end[piece_id] * magic::DOWNSAMPLE))) {
        for (long count = 0L; count < magic::Align_Nfft; count++) {
            X1[count] = info.src.data[count + startr] * window[count];
            X2[count] = info.rec.data[count + startd] * window[count];
        }
        fft_ctx.real_fwd(X1, magic::Align_Nfft);
        fft_ctx.real_fwd(X2, magic::Align_Nfft);

        for (long count = 0L; count <= magic::Align_Nfft / 2; count++) {
            r1 = X1[count * 2];
            i1 = -X1[1 + (count * 2)];
            X1[count * 2] = (r1 * X2[count * 2] - i1 * X2[1 + (count * 2)]);
            X1[1 + (count * 2)] = (r1 * X2[1 + (count * 2)] + i1 * X2[count * 2]);
        }

        fft_ctx.real_inv(X1, magic::Align_Nfft);

        v_max = 0.0f;
        for (long count = 0L; count < magic::Align_Nfft; count++) {
            r1 = (float) std::abs(X1[count]);
            X1[count] = r1;
            if (r1 > v_max) v_max = r1;
        }
        v_max *= 0.99f;
        for (long count = 0L; count < magic::Align_Nfft; count++)
            if (X1[count] > v_max)
                H[count] += (float) pow(v_max, 0.125);

        startr += (magic::Align_Nfft / 4);
        startd += (magic::Align_Nfft / 4);
    }

    Hsum = 0.0f;
    for (long count = 0L; count < magic::Align_Nfft; count++) {
        Hsum += H[count];
        X1[count] = H[count];
        X2[count] = 0.0f;
    }

    X2[0] = 1.0f;
    kernel = magic::Align_Nfft / 64;
    for (long count = 1; count < kernel; count++) {
        X2[count] = 1.0f - ((float) count) / ((float) kernel);
        X2[(magic::Align_Nfft - count)] = 1.0f - ((float) count) / ((float) kernel);
    }

    fft_ctx.real_fwd(X1, magic::Align_Nfft);
    fft_ctx.real_fwd(X2, magic::Align_Nfft);

    for (long count = 0L; count <= magic::Align_Nfft / 2; count++) {
        r1 = X1[count * 2];
        i1 = X1[1 + (count * 2)];
        X1[count * 2] = (r1 * X2[count * 2] - i1 * X2[1 + (count * 2)]);
        X1[1 + (count * 2)] = (r1 * X2[1 + (count * 2)] + i1 * X2[count * 2]);
    }
    fft_ctx.real_inv(X1, magic::Align_Nfft);

    for (long count = 0L; count < magic::Align_Nfft; count++) {
        if (Hsum > 0.0)
            H[count] = (float) std::abs(X1[count]) / Hsum;
        else
            H[count] = 0.0f;
    }

    v_max = 0.0f;
    I_max = 0L;
    for (long count = 0L; count < magic::Align_Nfft; count++)
        if (H[count] > v_max) {
            v_max = H[count];
            I_max = count;
        }
    if (I_max >= (magic::Align_Nfft / 2))
        I_max -= magic::Align_Nfft;

    info.piece_delay[piece_id] = estdelay + I_max;
    info.piece_delay_confidence[piece_id] = v_max;
}

void split_align(RatingContext& info, Signal ftmp,
                 long piece_start, long piece_speech_start, long piece_speech_end, long piece_end,
                 long piece_delay_est, float piece_delay_conf,
                 long* best_ED1, long* best_D1, float* best_DC1, long* best_ED2,
                 long* best_D2, float* best_DC2, long* best_BP) {
    long bp, k;
    long piece_len = piece_speech_end - piece_speech_start;
    long piece_test = magic::MAX_PIECES - 1;

    long n_bps;
    long piece_bps[41];
    long piece_ED1[41], piece_ED2[41];
    long piece_D1[41], piece_D2[41];
    float piece_DC1[41], piece_DC2[41];

    long delta, step, pad;

    long estdelay;
    long I_max;
    float v_max, n_max;
    long startr;
    long startd;
    float* window;
    float r1, i1;
    long kernel;
    float Hsum;

    *best_DC1 = 0.0f;
    *best_DC2 = 0.0f;

    float* X1 = ftmp.begin();
    float* X2 = ftmp.begin() + 2 + magic::Align_Nfft;
    float* H = (ftmp.begin() + 4 + 2 * magic::Align_Nfft);
    window = ftmp.begin() + 6 + 3 * magic::Align_Nfft;
    for (long count = 0L; count < magic::Align_Nfft; count++)
        window[count] = (float) (0.5 * (1.0 - std::cos((magic::TWOPI * count) / magic::Align_Nfft)));
    kernel = magic::Align_Nfft / 64;

    delta = magic::Align_Nfft / (4 * magic::DOWNSAMPLE);

    step = (long) ((0.801 * piece_len + 40 * delta - 1) / (40 * delta));
    step *= delta;

    pad = piece_len / 10;
    if (pad < 75) pad = 75;
    piece_bps[0] = piece_speech_start + pad;
    n_bps = 0;
    do {
        n_bps++;
        piece_bps[n_bps] = piece_bps[n_bps - 1] + step;
    } while ((piece_bps[n_bps] <= (piece_speech_end - pad)) && (n_bps < 40));

    if (n_bps <= 0) return;

    for (bp = 0; bp < n_bps; bp++) {
        info.piece_delay_est[piece_test] = piece_delay_est;
        info.piece_search_start[piece_test] = piece_start;
        info.piece_search_end[piece_test] = piece_bps[bp];

        crude_align(info, magic::MAX_PIECES, ftmp);
        piece_ED1[bp] = info.piece_delay[piece_test];

        info.piece_delay_est[piece_test] = piece_delay_est;
        info.piece_search_start[piece_test] = piece_bps[bp];
        info.piece_search_end[piece_test] = piece_end;

        crude_align(info, magic::MAX_PIECES, ftmp);
        piece_ED2[bp] = info.piece_delay[piece_test];
    }

    for (bp = 0; bp < n_bps; bp++)
        piece_DC1[bp] = -2.0f;

    dsp::FFTContext fft_ctx;
    while (true) {
        bp = 0;
        while ((bp < n_bps) && (piece_DC1[bp] > -2.0))
            bp++;
        if (bp >= n_bps)
            break;

        estdelay = piece_ED1[bp];

        for (long count = 0L; count < magic::Align_Nfft; count++)
            H[count] = 0.0f;
        Hsum = 0.0f;

        startr = piece_start * magic::DOWNSAMPLE;
        startd = startr + estdelay;

        if (startd < 0L) {
            startr = -estdelay;
            startd = 0L;
        }

        while (((startd + magic::Align_Nfft) <= info.rec.n_samples) &&
               ((startr + magic::Align_Nfft) <= (piece_bps[bp] * magic::DOWNSAMPLE))) {
            for (long count = 0L; count < magic::Align_Nfft; count++) {
                X1[count] = info.src.data[count + startr] * window[count];
                X2[count] = info.rec.data[count + startd] * window[count];
            }
            fft_ctx.real_fwd(X1, magic::Align_Nfft);
            fft_ctx.real_fwd(X2, magic::Align_Nfft);

            for (long count = 0L; count <= magic::Align_Nfft / 2; count++) {
                r1 = X1[count * 2];
                i1 = -X1[1 + (count * 2)];
                X1[count * 2] = (r1 * X2[count * 2] - i1 * X2[1 + (count * 2)]);
                X1[1 + (count * 2)] = (r1 * X2[1 + (count * 2)] + i1 * X2[count * 2]);
            }

            fft_ctx.real_inv(X1, magic::Align_Nfft);

            v_max = 0.0f;
            for (long count = 0L; count < magic::Align_Nfft; count++) {
                r1 = (float) std::abs(X1[count]);
                X1[count] = r1;
                if (r1 > v_max) v_max = r1;
            }
            v_max *= 0.99f;
            n_max = (float) pow(v_max, 0.125) / kernel;

            for (long count = 0L; count < magic::Align_Nfft; count++)
                if (X1[count] > v_max) {
                    Hsum += n_max * kernel;
                    for (k = 1 - kernel; k < kernel; k++)
                        H[(count + k + magic::Align_Nfft) % magic::Align_Nfft] +=
                            n_max * (kernel - (float) std::abs(k));
                }

            startr += (magic::Align_Nfft / 4);
            startd += (magic::Align_Nfft / 4);
        }

        v_max = 0.0f;
        I_max = 0L;
        for (long count = 0L; count < magic::Align_Nfft; count++)
            if (H[count] > v_max) {
                v_max = H[count];
                I_max = count;
            }
        if (I_max >= (magic::Align_Nfft / 2))
            I_max -= magic::Align_Nfft;

        piece_D1[bp] = estdelay + I_max;
        if (Hsum > 0.0)
            piece_DC1[bp] = v_max / Hsum;
        else
            piece_DC1[bp] = 0.0f;

        while (bp < (n_bps - 1)) {
            bp++;
            if ((piece_ED1[bp] == estdelay) && (piece_DC1[bp] <= -2.0)) {
                while (((startd + magic::Align_Nfft) <= info.rec.n_samples) &&
                       ((startr + magic::Align_Nfft) <= (piece_bps[bp] * magic::DOWNSAMPLE))) {
                    for (long count = 0L; count < magic::Align_Nfft; count++) {
                        X1[count] = info.src.data[count + startr] * window[count];
                        X2[count] = info.rec.data[count + startd] * window[count];
                    }
                    fft_ctx.real_fwd(X1, magic::Align_Nfft);
                    fft_ctx.real_fwd(X2, magic::Align_Nfft);

                    for (long count = 0L; count <= magic::Align_Nfft / 2; count++) {
                        r1 = X1[count * 2];
                        i1 = -X1[1 + (count * 2)];
                        X1[count * 2] = (r1 * X2[count * 2] - i1 * X2[1 + (count * 2)]);
                        X1[1 + (count * 2)] = (r1 * X2[1 + (count * 2)] + i1 * X2[count * 2]);
                    }

                    fft_ctx.real_inv(X1, magic::Align_Nfft);

                    v_max = 0.0f;
                    for (long count = 0L; count < magic::Align_Nfft; count++) {
                        r1 = (float) std::abs(X1[count]);
                        X1[count] = r1;
                        if (r1 > v_max) v_max = r1;
                    }
                    v_max *= 0.99f;
                    n_max = (float) pow(v_max, 0.125) / kernel;

                    for (long count = 0L; count < magic::Align_Nfft; count++)
                        if (X1[count] > v_max) {
                            Hsum += n_max * kernel;
                            for (k = 1 - kernel; k < kernel; k++)
                                H[(count + k + magic::Align_Nfft) % magic::Align_Nfft] +=
                                    n_max * (kernel - (float) std::abs(k));
                        }

                    startr += (magic::Align_Nfft / 4);
                    startd += (magic::Align_Nfft / 4);
                }

                v_max = 0.0f;
                I_max = 0L;
                for (long count = 0L; count < magic::Align_Nfft; count++)
                    if (H[count] > v_max) {
                        v_max = H[count];
                        I_max = count;
                    }
                if (I_max >= (magic::Align_Nfft / 2))
                    I_max -= magic::Align_Nfft;

                piece_D1[bp] = estdelay + I_max;
                if (Hsum > 0.0)
                    piece_DC1[bp] = v_max / Hsum;
                else
                    piece_DC1[bp] = 0.0f;
            }
        }
    }

    for (bp = 0; bp < n_bps; bp++) {
        if (piece_DC1[bp] > piece_delay_conf)
            piece_DC2[bp] = -2.0f;
        else
            piece_DC2[bp] = 0.0f;
    }
    while (true) {
        bp = n_bps - 1;
        while ((bp >= 0) && (piece_DC2[bp] > -2.0))
            bp--;
        if (bp < 0)
            break;

        estdelay = piece_ED2[bp];

        for (long count = 0L; count < magic::Align_Nfft; count++)
            H[count] = 0.0f;
        Hsum = 0.0f;

        startr = piece_end * magic::DOWNSAMPLE - magic::Align_Nfft;
        startd = startr + estdelay;

        if ((startd + magic::Align_Nfft) > info.rec.n_samples) {
            startd = info.rec.n_samples - magic::Align_Nfft;
            startr = startd - estdelay;
        }

        while ((startd >= 0L) &&
               (startr >= (piece_bps[bp] * magic::DOWNSAMPLE))) {
            for (long count = 0L; count < magic::Align_Nfft; count++) {
                X1[count] = info.src.data[count + startr] * window[count];
                X2[count] = info.rec.data[count + startd] * window[count];
            }
            fft_ctx.real_fwd(X1, magic::Align_Nfft);
            fft_ctx.real_fwd(X2, magic::Align_Nfft);

            for (long count = 0L; count <= magic::Align_Nfft / 2; count++) {
                r1 = X1[count * 2];
                i1 = -X1[1 + (count * 2)];
                X1[count * 2] = (r1 * X2[count * 2] - i1 * X2[1 + (count * 2)]);
                X1[1 + (count * 2)] = (r1 * X2[1 + (count * 2)] + i1 * X2[count * 2]);
            }

            fft_ctx.real_inv(X1, magic::Align_Nfft);

            v_max = 0.0f;
            for (long count = 0L; count < magic::Align_Nfft; count++) {
                r1 = (float) std::abs(X1[count]);
                X1[count] = r1;
                if (r1 > v_max) v_max = r1;
            }
            v_max *= 0.99f;
            n_max = (float) pow(v_max, 0.125) / kernel;

            for (long count = 0L; count < magic::Align_Nfft; count++)
                if (X1[count] > v_max) {
                    Hsum += n_max * kernel;
                    for (k = 1 - kernel; k < kernel; k++)
                        H[(count + k + magic::Align_Nfft) % magic::Align_Nfft] +=
                            n_max * (kernel - (float) std::abs(k));
                }

            startr -= (magic::Align_Nfft / 4);
            startd -= (magic::Align_Nfft / 4);
        }

        v_max = 0.0f;
        I_max = 0L;
        for (long count = 0L; count < magic::Align_Nfft; count++)
            if (H[count] > v_max) {
                v_max = H[count];
                I_max = count;
            }
        if (I_max >= (magic::Align_Nfft / 2))
            I_max -= magic::Align_Nfft;

        piece_D2[bp] = estdelay + I_max;
        if (Hsum > 0.0)
            piece_DC2[bp] = v_max / Hsum;
        else
            piece_DC2[bp] = 0.0f;

        while (bp > 0) {
            bp--;
            if ((piece_ED2[bp] == estdelay) && (piece_DC2[bp] <= -2.0)) {
                while ((startd >= 0L) &&
                       (startr >= (piece_bps[bp] * magic::DOWNSAMPLE))) {
                    for (long count = 0L; count < magic::Align_Nfft; count++) {
                        X1[count] = info.src.data[count + startr] * window[count];
                        X2[count] = info.rec.data[count + startd] * window[count];
                    }
                    fft_ctx.real_fwd(X1, magic::Align_Nfft);
                    fft_ctx.real_fwd(X2, magic::Align_Nfft);

                    for (long count = 0L; count <= magic::Align_Nfft / 2; count++) {
                        r1 = X1[count * 2];
                        i1 = -X1[1 + (count * 2)];
                        X1[count * 2] = (r1 * X2[count * 2] - i1 * X2[1 + (count * 2)]);
                        X1[1 + (count * 2)] = (r1 * X2[1 + (count * 2)] + i1 * X2[count * 2]);
                    }

                    fft_ctx.real_inv(X1, magic::Align_Nfft);

                    v_max = 0.0f;
                    for (long count = 0L; count < magic::Align_Nfft; count++) {
                        r1 = (float) std::abs(X1[count]);
                        X1[count] = r1;
                        if (r1 > v_max) v_max = r1;
                    }
                    v_max *= 0.99f;
                    n_max = (float) pow(v_max, 0.125) / kernel;

                    for (long count = 0L; count < magic::Align_Nfft; count++)
                        if (X1[count] > v_max) {
                            Hsum += n_max * kernel;
                            for (k = 1 - kernel; k < kernel; k++)
                                H[(count + k + magic::Align_Nfft) % magic::Align_Nfft] +=
                                    n_max * (kernel - (float) std::abs(k));
                        }

                    startr -= (magic::Align_Nfft / 4);
                    startd -= (magic::Align_Nfft / 4);
                }

                v_max = 0.0f;
                I_max = 0L;
                for (long count = 0L; count < magic::Align_Nfft; count++)
                    if (H[count] > v_max) {
                        v_max = H[count];
                        I_max = count;
                    }
                if (I_max >= (magic::Align_Nfft / 2))
                    I_max -= magic::Align_Nfft;

                piece_D2[bp] = estdelay + I_max;
                if (Hsum > 0.0)
                    piece_DC2[bp] = v_max / Hsum;
                else
                    piece_DC2[bp] = 0.0f;
            }
        }
    }

    for (bp = 0; bp < n_bps; bp++) {
        if ((std::abs(piece_D2[bp] - piece_D1[bp]) >= magic::DOWNSAMPLE) &&
            ((piece_DC1[bp] + piece_DC2[bp]) > ((*best_DC1) + (*best_DC2))) &&
            (piece_DC1[bp] > piece_delay_conf) && (piece_DC2[bp] > piece_delay_conf)) {
            *best_ED1 = piece_ED1[bp];
            *best_D1 = piece_D1[bp];
            *best_DC1 = piece_DC1[bp];
            *best_ED2 = piece_ED2[bp];
            *best_D2 = piece_D2[bp];
            *best_DC2 = piece_DC2[bp];
            *best_BP = piece_bps[bp];
        }
    }
}

}