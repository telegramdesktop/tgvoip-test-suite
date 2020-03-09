#include <cmath>
#include "processing.hpp"
#include "magic.hpp"
#include "dsp.hpp"



namespace tgvoipcontest {

static int Nb;

void input_filter(SignalInfo& info) {
    dc_block(info.data, info.n_samples);
    apply_filters(info.data);
}

void calc_VAD(const SignalInfo& sinfo) {
    apply_VAD(sinfo.n_samples, sinfo.data, sinfo.VAD, sinfo.logVAD);
}

int id_searchwindows(RatingContext& info) {
    long num_pieces = 0;
    long this_start = 0;

    long VAD_length = info.src.n_samples / magic::DOWNSAMPLE;

    long del_deg_start = magic::MIN_PIECE_LEN - info.crude_delay / magic::DOWNSAMPLE;
    long del_deg_end =
        (info.rec.n_samples - info.crude_delay) / magic::DOWNSAMPLE - magic::MIN_PIECE_LEN;
    bool speech_flag = false;

    for (long count = 0; count < VAD_length; ++count) {
        float VAD_value = info.src.VAD[count];

        if (VAD_value > 0.0f && !speech_flag) {
            speech_flag = true;
            this_start = count;
            info.piece_search_start[num_pieces] = std::max(0l, static_cast<long>(count) - magic::SEARCHBUFFER);
        }

        if ((VAD_value == 0.0f || count == (VAD_length - 1)) && speech_flag) {
            speech_flag = false;
            info.piece_search_end[num_pieces] = count + magic::SEARCHBUFFER;
            if (info.piece_search_end[num_pieces] > VAD_length - 1)
                info.piece_search_end[num_pieces] = VAD_length - 1;

            if ((count - this_start) >= magic::MIN_PIECE_LEN &&
                this_start < del_deg_end && static_cast<long>(count) > del_deg_start) {
                num_pieces++;
            }
        }
    }

    info.n_pieces = num_pieces;
    return num_pieces;
}

void id_pieces(RatingContext& info) {
    long piece_num = 0;
    long largest_piece_size = 0;
    int speech_flag = 0;
    float VAD_value;
    long this_start = 0;
    long last_end;

    long VAD_length = info.src.n_samples / magic::DOWNSAMPLE;

    long del_deg_start = magic::MIN_PIECE_LEN - info.crude_delay / magic::DOWNSAMPLE;
    long del_deg_end =
        (info.rec.n_samples - info.crude_delay) / magic::DOWNSAMPLE - magic::MIN_PIECE_LEN;

    for (long count = 0; count < VAD_length; count++) {
        VAD_value = info.src.VAD[count];
        if ((VAD_value > 0.0f) && (speech_flag == 0)) {
            speech_flag = 1;
            this_start = count;
            info.piece_start[piece_num] = count;
        }

        if (((VAD_value == 0.0f) || (count == (VAD_length - 1))) &&
            (speech_flag == 1)) {
            speech_flag = 0;
            info.piece_end[piece_num] = count;

            if (((count - this_start) >= magic::MIN_PIECE_LEN) &&
                (static_cast<long>(this_start) < del_deg_end) &&
                (static_cast<long>(count) > del_deg_start))
                piece_num++;
        }
    }

    info.piece_start[0] = magic::SEARCHBUFFER;
    info.piece_end[info.n_pieces - 1] = (VAD_length - magic::SEARCHBUFFER);

    long count;
    for (piece_num = 1; piece_num < info.n_pieces; piece_num++) {
        this_start = info.piece_start[piece_num];
        last_end = info.piece_end[piece_num - 1];
        count = (this_start + last_end) / 2;
        info.piece_start[piece_num] = count;
        info.piece_end[piece_num - 1] = count;
    }

    this_start = (info.piece_start[0] * magic::DOWNSAMPLE) + info.piece_delay[0];
    if (this_start < (magic::SEARCHBUFFER * magic::DOWNSAMPLE)) {
        count = magic::SEARCHBUFFER +
                (magic::DOWNSAMPLE - 1 - info.piece_delay[0]) / magic::DOWNSAMPLE;
        info.piece_start[0] = count;
    }
    last_end = (info.piece_end[info.n_pieces - 1] * magic::DOWNSAMPLE) +
               info.piece_delay[info.n_pieces - 1];
    if (last_end > (info.rec.n_samples - magic::SEARCHBUFFER * magic::DOWNSAMPLE)) {
        count = (info.rec.n_samples -
                 info.piece_delay[info.n_pieces - 1]) / magic::DOWNSAMPLE -
                magic::SEARCHBUFFER;
        info.piece_end[info.n_pieces - 1] = count;
    }

    for (piece_num = 1; piece_num < info.n_pieces; piece_num++) {
        this_start =
            (info.piece_start[piece_num] * magic::DOWNSAMPLE) +
            info.piece_delay[piece_num];
        last_end =
            (info.piece_end[piece_num - 1] * magic::DOWNSAMPLE) +
            info.piece_delay[piece_num - 1];
        if (this_start < last_end) {
            count = (this_start + last_end) / 2;
            this_start =
                (magic::DOWNSAMPLE - 1 + count - info.piece_delay[piece_num]) / magic::DOWNSAMPLE;
            last_end =
                (count - info.piece_delay[piece_num - 1]) / magic::DOWNSAMPLE;
            info.piece_start[piece_num] = this_start;
            info.piece_end[piece_num - 1] = last_end;
        }
    }

    for (piece_num = 0; piece_num < info.n_pieces; piece_num++)
        if ((info.piece_end[piece_num] - info.piece_start[piece_num])
            > largest_piece_size)
            largest_piece_size =
                info.piece_end[piece_num] - info.piece_start[piece_num];
}

void pieces_split(RatingContext& info, Signal ftmp) {
    long best_ED1, best_ED2;
    long best_D1, best_D2;
    float best_DC1, best_DC2;
    long best_BP;
    long largest_piece_size = 0;

    long piece_id = 0;
    while ((piece_id < info.n_pieces) &&
           (info.n_pieces < magic::MAX_PIECES)) {
        long piece_delay_est = info.piece_delay_est[piece_id];
        float piece_delay_confidence = info.piece_delay_confidence[piece_id];
        long piece_start = info.piece_start[piece_id];
        long piece_end = info.piece_end[piece_id];

        long piece_speech_start = piece_start;
        while ((piece_speech_start < piece_end) && (info.src.VAD[piece_speech_start] <= 0.0f))
            piece_speech_start++;
        long piece_speech_end = piece_end;
        while ((piece_speech_end > piece_start) && (info.src.VAD[piece_speech_end] <= 0.0f))
            piece_speech_end--;
        piece_speech_end++;
        long piece_len = piece_speech_end - piece_speech_start;

        if (piece_len >= 200) {
            split_align(info, ftmp,
                        piece_start, piece_speech_start, piece_speech_end, piece_end,
                        piece_delay_est, piece_delay_confidence,
                        &best_ED1, &best_D1, &best_DC1,
                        &best_ED2, &best_D2, &best_DC2,
                        &best_BP);

            if ((best_DC1 > piece_delay_confidence) && (best_DC2 > piece_delay_confidence)) {
                for (long step = info.n_pieces - 1; step > piece_id; step--) {
                    info.piece_delay_est[step + 1] = info.piece_delay_est[step];
                    info.piece_delay[step + 1] = info.piece_delay[step];
                    info.piece_delay_confidence[step + 1] = info.piece_delay_confidence[step];
                    info.piece_start[step + 1] = info.piece_start[step];
                    info.piece_end[step + 1] = info.piece_end[step];
                    info.piece_search_start[step + 1] = info.piece_start[step];
                    info.piece_search_end[step + 1] = info.piece_end[step];
                }
                info.n_pieces++;

                info.piece_delay_est[piece_id] = best_ED1;
                info.piece_delay[piece_id] = best_D1;
                info.piece_delay_confidence[piece_id] = best_DC1;

                info.piece_delay_est[piece_id + 1] = best_ED2;
                info.piece_delay[piece_id + 1] = best_D2;
                info.piece_delay_confidence[piece_id + 1] = best_DC2;

                info.piece_search_start[piece_id + 1] = info.piece_search_start[piece_id];
                info.piece_search_end[piece_id + 1] = info.piece_search_end[piece_id];

                if (best_D2 < best_D1) {
                    info.piece_start[piece_id] = piece_start;
                    info.piece_end[piece_id] = best_BP;
                    info.piece_start[piece_id + 1] = best_BP;
                    info.piece_end[piece_id + 1] = piece_end;
                } else {
                    info.piece_start[piece_id] = piece_start;
                    info.piece_end[piece_id] = best_BP + (best_D2 - best_D1) / (2 * magic::DOWNSAMPLE);
                    info.piece_start[piece_id + 1] = best_BP - (best_D2 - best_D1) / (2 * magic::DOWNSAMPLE);
                    info.piece_end[piece_id + 1] = piece_end;
                }

                if ((info.piece_start[piece_id] - magic::SEARCHBUFFER) * magic::DOWNSAMPLE + best_D1 < 0)
                    info.piece_start[piece_id] =
                        magic::SEARCHBUFFER + (magic::DOWNSAMPLE - 1 - best_D1) / magic::DOWNSAMPLE;

                if ((info.piece_end[piece_id + 1] * magic::DOWNSAMPLE + best_D2) >
                    (info.rec.n_samples - magic::SEARCHBUFFER * magic::DOWNSAMPLE))
                    info.piece_end[piece_id + 1] =
                        (info.rec.n_samples - best_D2) / magic::DOWNSAMPLE - magic::SEARCHBUFFER;

            } else piece_id++;
        } else piece_id++;
    }

    for (piece_id = 0; piece_id < info.n_pieces; piece_id++)
        if ((info.piece_end[piece_id] - info.piece_start[piece_id]) > largest_piece_size)
            largest_piece_size = info.piece_end[piece_id] - info.piece_start[piece_id];
}

void pieces_locate(RatingContext& info, Signal ftmp) {
    id_searchwindows(info);

    for (long piece_id = 0; piece_id < info.n_pieces; piece_id++) {
        crude_align(info, piece_id, ftmp);
        time_align(info, piece_id, ftmp);
    }

    id_pieces(info);

    pieces_split(info, ftmp);
}


void short_term_fft(
    dsp::FFTContext& fft_ctx,
    long Nf, const SignalInfo& info, const float* window, long start_sample, Signal hz_spectrum, Signal fft_tmp
) {
    for (long n = 0; n < Nf; n++) {
        fft_tmp[n] = info.data[start_sample + n] * window[n];
    }
    fft_ctx.real_fwd(fft_tmp.begin(), Nf);

    for (long k = 0; k < Nf / 2; k++) {
        hz_spectrum[k] = fft_tmp[k << 1] * fft_tmp[k << 1] + fft_tmp[1 + (k << 1)] * fft_tmp[1 + (k << 1)];
    }

    hz_spectrum[0] = 0;
}

void freq_warping(Signal hz_spectrum, int Nb, Signal pitch_pow_dens, long frame) {
    int hz_band = 0;

    for (int bark_band = 0; bark_band < Nb; bark_band++) {
        int n = magic::nr_of_hz_bands_per_bark_band[bark_band];
        int i;

        double sum = 0;
        for (i = 0; i < n; i++) {
            sum += hz_spectrum[hz_band++];
        }

        sum *= magic::pow_dens_correction_factor[bark_band];
        sum *= magic::Sp;
        pitch_pow_dens[frame * Nb + bark_band] = (float) sum;
    }
}

float total_audible(int frame, Signal pitch_pow_dens, float factor) {
    double result = 0.;

    for (int band = 1; band < Nb; band++) {
        float h = pitch_pow_dens[frame * Nb + band];
        float threshold = (float) (factor * magic::abs_thresh_power[band]);
        if (h > threshold) {
            result += h;
        }
    }
    return (float) result;
}

void time_avg_audible_of(int number_of_frames, Series<int> silent, Signal pitch_pow_dens, Signal avg_pitch_pow_dens,
                         int total_number_of_frames) {
    for (int band = 0; band < Nb; band++) {
        double result = 0;
        for (int frame = 0; frame < number_of_frames; frame++) {
            if (!silent[frame]) {
                float h = pitch_pow_dens[frame * Nb + band];
                if (h > 100 * magic::abs_thresh_power[band]) {
                    result += h;
                }
            }
        }

        avg_pitch_pow_dens[band] = (float) (result / total_number_of_frames);
    }
}

void intensity_warping_of(Signal loudness_dens, int frame, Signal pitch_pow_dens) {
    float h;

    for (int band = 0; band < Nb; band++) {
        float threshold = (float) magic::abs_thresh_power[band];
        float input = pitch_pow_dens[frame * Nb + band];

        if (magic::centre_of_band_bark[band] < 4.0f) {
            h = 6.0f / ((float) magic::centre_of_band_bark[band] + 2.0f);
        } else {
            h = 1.0f;
        }
        if (h > (float) 2) {
            h = 2.0f;
        }
        h = (float) std::pow(h, 0.15f);
        double modified_zwicker_power = magic::ZWICKER_POWER * h;

        if (input > threshold) {
            loudness_dens[band] = (float) (std::pow(threshold / 0.5, modified_zwicker_power)
                                           * (std::pow(0.5 + 0.5 * input / threshold, modified_zwicker_power) - 1));
        } else {
            loudness_dens[band] = 0;
        }

        loudness_dens[band] *= (float) magic::Sl;
    }
}

float pseudo_Lp(Signal x, float p) {
    double totalWeight = 0;
    double result = 0;

    for (size_t band = 1; band < x.size(); band++) {
        float h = std::abs(x[band]);
        float w = magic::width_of_band_bark[band];
        float prod = h * w;

        result += std::pow(prod, p);
        totalWeight += w;
    }

    result /= totalWeight;
    result = std::pow(result, 1 / p);
    result *= totalWeight;

    return (float) result;
}

void multiply_with_asymmetry_factor(Signal disturbance_dens,
                                    int frame,
                                    const Signal pitch_pow_dens_ref,
                                    const Signal pitch_pow_dens_deg) {
    for (int i = 0; i < Nb; i++) {
        float ratio = (pitch_pow_dens_deg[frame * Nb + i] + (float) 50)
                / (pitch_pow_dens_ref[frame * Nb + i] + (float) 50);

        float h = (float) std::pow(ratio, (float) 1.2);
        if (h > (float) 12) { h = (float) 12; }
        if (h < (float) 3) { h = (float) 0.0; }

        disturbance_dens[i] *= h;
    }
}

double pow_of(const Signal  x, long start_sample, long stop_sample, long divisor) {
    long double power = 0.0;

    if (start_sample < 0 || start_sample > stop_sample) {
        throw RatingModelException{"Internal"};
    }

    for (long i = start_sample; i < stop_sample; i++) {
        long double h = x[i];
        power += h * h;
    }

    power /= divisor;
    return power;
}


int compute_delay(long start_sample,
                  long stop_sample,
                  long search_range,
                  const Signal time_series1,
                  const Signal time_series2,
                  float* max_correlation) {

    long n = stop_sample - start_sample;
    long power_of_2 = dsp::nextpow2(2 * n);

    double power1 = pow_of(
        time_series1, start_sample, stop_sample, stop_sample - start_sample) * (double) n / (double) power_of_2;
    double power2 = pow_of(
        time_series2, start_sample, stop_sample, stop_sample - start_sample) * (double) n / (double) power_of_2;
    double normalization = sqrt(power1 * power2);

    if ((power1 <= 1E-6) || (power2 <= 1E-6)) {
        *max_correlation = 0;
        return 0;
    }

    auto x1 = Signal(power_of_2 + 2);
    auto x2 = Signal(power_of_2 + 2);
    auto y = Signal(power_of_2 + 2);

    for (long i = 0; i < power_of_2 + 2; i++) {
        x1[i] = 0.;
        x2[i] = 0.;
        y[i] = 0.;
    }

    for (long i = 0; i < n; i++) {
        x1[i] = (float) std::abs(time_series1[i + start_sample]);
        x2[i] = (float) std::abs(time_series2[i + start_sample]);
    }

    dsp::FFTContext fft_ctx;
    fft_ctx.real_fwd(x1.begin(), power_of_2);
    fft_ctx.real_fwd(x2.begin(), power_of_2);

    for (long i = 0; i <= power_of_2 / 2; i++) {
        x1[2 * i] /= power_of_2;
        x1[2 * i + 1] /= power_of_2;
    }

    for (long i = 0; i <= power_of_2 / 2; i++) {
        y[2 * i] = x1[2 * i] * x2[2 * i] + x1[2 * i + 1] * x2[2 * i + 1];
        y[2 * i + 1] = -x1[2 * i + 1] * x2[2 * i] + x1[2 * i] * x2[2 * i + 1];
    }

    fft_ctx.real_inv(y.begin(), power_of_2);

    long best_delay = 0;
    *max_correlation = 0;

    for (long i = -search_range; i <= -1; i++) {
        double h = std::abs(y[(i + power_of_2)]) / normalization;
        if (std::abs(h) > (double) *max_correlation) {
            *max_correlation = (float) std::abs(h);
            best_delay = i;
        }
    }

    for (long i = 0; i < search_range; i++) {
        double h = std::abs(y[i]) / normalization;
        if (std::abs(h) > (double) *max_correlation) {
            *max_correlation = (float) std::abs(h);
            best_delay = i;
        }
    }

    return best_delay;
}


float Lpq_weight(int start_frame,
                 int stop_frame,
                 float power_syllable,
                 float power_time,
                 const Signal frame_disturbance,
                 const Signal time_weight) {

    double result_time = 0;
    double total_time_weight_time = 0;
    int start_frame_of_syllable;

    for (start_frame_of_syllable = start_frame;
         start_frame_of_syllable <= stop_frame;
         start_frame_of_syllable += magic::NUMBER_OF_PSQM_FRAMES_PER_SYLLABE / 2) {

        double result_syllable = 0;
        int count_syllable = 0;
        int frame;

        for (frame = start_frame_of_syllable;
             frame < start_frame_of_syllable + magic::NUMBER_OF_PSQM_FRAMES_PER_SYLLABE;
             frame++) {
            if (frame <= stop_frame) {
                float h = frame_disturbance[frame];
                result_syllable += std::pow(h, power_syllable);
            }
            count_syllable++;
        }

        result_syllable /= count_syllable;
        result_syllable = std::pow(result_syllable, (double) 1 / power_syllable);

        result_time += std::pow(time_weight[start_frame_of_syllable - start_frame] * result_syllable, power_time);
        total_time_weight_time += std::pow(time_weight[start_frame_of_syllable - start_frame], power_time);
    }

    result_time /= total_time_weight_time;
    result_time = std::pow(result_time, (float) 1 / power_time);

    return (float) result_time;
}

void voip_qos_model(RatingContext& info) {
    long max_n_samples = std::max(info.src.n_samples, info.rec.n_samples);
    long Nf = magic::DOWNSAMPLE * 8L;
    long start_frame, stop_frame;
    long samples_to_skip_at_start, samples_to_skip_at_end;
    float sum_of_5_samples;
    float oldScale, scale;

    int start_frame_of_bad_interval[magic::MAX_NUMBER_OF_BAD_INTERVALS];
    int stop_frame_of_bad_interval[magic::MAX_NUMBER_OF_BAD_INTERVALS];
    int start_sample_of_bad_interval[magic::MAX_NUMBER_OF_BAD_INTERVALS];
    int stop_sample_of_bad_interval[magic::MAX_NUMBER_OF_BAD_INTERVALS];
    int number_of_samples_in_bad_interval[magic::MAX_NUMBER_OF_BAD_INTERVALS];
    int delay_in_samples_in_bad_interval[magic::MAX_NUMBER_OF_BAD_INTERVALS];
    int number_of_bad_intervals = 0;
    int search_range_in_samples;
    int bad_interval;
    int there_is_a_bad_frame = false;
    float d_indicator, a_indicator;

    float w_hanning[magic::Nfmax];

    for (long n = 0L; n < Nf; n++) {
        w_hanning[n] = (float) (0.5 * (1.0 - std::cos((magic::TWOPI * n) / Nf)));
    }

    Nb = 49;

    samples_to_skip_at_start = 0;
    do {
        sum_of_5_samples = (float) 0;
        for (long i = 0; i < 5; i++) {
            sum_of_5_samples += (float) std::abs(
                info.src.data[magic::SEARCHBUFFER * magic::DOWNSAMPLE + samples_to_skip_at_start + i]);
        }
        if (sum_of_5_samples < magic::CRITERIUM_FOR_SILENCE_OF_5_SAMPLES) {
            samples_to_skip_at_start++;
        }
    } while ((sum_of_5_samples < magic::CRITERIUM_FOR_SILENCE_OF_5_SAMPLES)
             && (samples_to_skip_at_start < max_n_samples / 2));

    samples_to_skip_at_end = 0;
    do {
        sum_of_5_samples = (float) 0;
        for (long i = 0; i < 5; i++) {
            sum_of_5_samples += (float) std::abs(
                info.src.data[max_n_samples - magic::SEARCHBUFFER * magic::DOWNSAMPLE +
                               magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS - 1 -
                               samples_to_skip_at_end - i]);
        }
        if (sum_of_5_samples < magic::CRITERIUM_FOR_SILENCE_OF_5_SAMPLES) {
            samples_to_skip_at_end++;
        }
    } while ((sum_of_5_samples < magic::CRITERIUM_FOR_SILENCE_OF_5_SAMPLES)
             && (samples_to_skip_at_end < max_n_samples / 2));

    start_frame = samples_to_skip_at_start / (Nf / 2);
    stop_frame =
        (max_n_samples - 2 * magic::SEARCHBUFFER * magic::DOWNSAMPLE + magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS -
         samples_to_skip_at_end) /
        (Nf / 2) - 1;

    auto fft_tmp = Signal(Nf + 2);
    auto hz_spectrum_ref = Signal(Nf / 2);
    auto hz_spectrum_deg = Signal(Nf / 2);

    auto frame_is_bad = Series<int>(stop_frame + 1);
    auto smeared_frame_is_bad = Series<int>(stop_frame + 1);

    auto silent = Series<int>(stop_frame + 1);

    auto pitch_pow_dens_ref = Signal((stop_frame + 1) * Nb);
    auto pitch_pow_dens_deg = Signal((stop_frame + 1) * Nb);

    auto frame_was_skipped = Series<int>(stop_frame + 1);

    auto frame_disturbance = Signal(stop_frame + 1);
    auto frame_disturbance_asym_add = Signal(stop_frame + 1);

    auto avg_pitch_pow_dens_ref = Signal(Nb);
    auto avg_pitch_pow_dens_deg = Signal(Nb);
    auto loudness_dens_ref = Signal(Nb);
    auto loudness_dens_deg = Signal(Nb);;
    auto deadzone = Signal(Nb);
    auto disturbance_dens = Signal(Nb);
    auto disturbance_dens_asym_add = Signal(Nb);

    auto time_weight = Signal(stop_frame + 1);
    auto total_power_ref = Signal(stop_frame + 1);

    dsp::FFTContext fft_ctx;
    for (long frame = 0; frame <= stop_frame; frame++) {
        int start_sample_ref = magic::SEARCHBUFFER * magic::DOWNSAMPLE + frame * Nf / 2;
        int start_sample_deg;
        int delay;

        short_term_fft(fft_ctx, Nf, info.src, w_hanning, start_sample_ref, hz_spectrum_ref, fft_tmp);

        if (info.n_pieces < 1) {
            throw RatingModelException{"Processing error!"};
        }

        long piece_id = info.n_pieces - 1;
        while ((piece_id >= 0) && (info.piece_start[piece_id] * magic::DOWNSAMPLE > start_sample_ref)) {
            piece_id--;
        }
        if (piece_id >= 0) {
            delay = info.piece_delay[piece_id];
        } else {
            delay = info.piece_delay[0];
        }
        start_sample_deg = start_sample_ref + delay;

        if ((start_sample_deg > 0) &&
            (start_sample_deg + Nf < max_n_samples + magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS)) {
            short_term_fft(fft_ctx, Nf, info.rec, w_hanning, start_sample_deg, hz_spectrum_deg, fft_tmp);
        } else {
            for (long i = 0; i < Nf / 2; i++) {
                hz_spectrum_deg[i] = 0;
            }
        }

        freq_warping(hz_spectrum_ref, Nb, pitch_pow_dens_ref, frame);

        freq_warping(hz_spectrum_deg, Nb, pitch_pow_dens_deg, frame);

        float total_audible_pow_ref = total_audible(frame, pitch_pow_dens_ref, 1E2);

        silent[frame] = (total_audible_pow_ref < 1E7);
    }

    time_avg_audible_of(stop_frame + 1, silent, pitch_pow_dens_ref, avg_pitch_pow_dens_ref,
                        (max_n_samples - 2 * magic::SEARCHBUFFER * magic::DOWNSAMPLE +
                         magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS) / (Nf / 2) - 1);
    time_avg_audible_of(stop_frame + 1, silent, pitch_pow_dens_deg, avg_pitch_pow_dens_deg,
                        (max_n_samples - 2 * magic::SEARCHBUFFER * magic::DOWNSAMPLE +
                         magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS) / (Nf / 2) - 1);

    oldScale = 1;
    for (long frame = 0; frame <= stop_frame; frame++) {
        int band;

        float total_audible_pow_ref = total_audible(frame, pitch_pow_dens_ref, 1);
        float total_audible_pow_deg = total_audible(frame, pitch_pow_dens_deg, 1);
        total_power_ref[frame] = total_audible_pow_ref;

        scale = (total_audible_pow_ref + (float) 5E3) / (total_audible_pow_deg + (float) 5E3);

        if (frame > 0) {
            scale = (float) 0.2 * oldScale + (float) 0.8 * scale;
        }
        oldScale = scale;

        if (scale > (float) magic::MAX_SCALE) scale = (float) magic::MAX_SCALE;

        if (scale < (float) magic::MIN_SCALE) {
            scale = (float) magic::MIN_SCALE;
        }

        for (band = 0; band < Nb; band++) {
            pitch_pow_dens_deg[frame * Nb + band] *= scale;
        }

        intensity_warping_of(loudness_dens_ref, frame, pitch_pow_dens_ref);
        intensity_warping_of(loudness_dens_deg, frame, pitch_pow_dens_deg);

        for (band = 0; band < Nb; band++) {
            disturbance_dens[band] = loudness_dens_deg[band] - loudness_dens_ref[band];
        }

        for (band = 0; band < Nb; band++) {
            deadzone[band] = std::min(loudness_dens_deg[band], loudness_dens_ref[band]);
            deadzone[band] *= 0.25;
        }

        for (band = 0; band < Nb; band++) {
            float d = disturbance_dens[band];
            float m = deadzone[band];

            if (d > m) {
                disturbance_dens[band] -= m;
            } else {
                if (d < -m) {
                    disturbance_dens[band] += m;
                } else {
                    disturbance_dens[band] = 0;
                }
            }
        }

        frame_disturbance[frame] = pseudo_Lp(disturbance_dens, magic::D_POW_F);

        if (frame_disturbance[frame] > magic::THRESHOLD_BAD_FRAMES) {
            there_is_a_bad_frame = true;
        }

        multiply_with_asymmetry_factor(disturbance_dens, frame, pitch_pow_dens_ref, pitch_pow_dens_deg);
        frame_disturbance_asym_add[frame] = pseudo_Lp(disturbance_dens, magic::A_POW_F);
    }

    for (long frame = 0; frame <= stop_frame; frame++) {
        frame_was_skipped[frame] = false;
    }

    for (long pc_id = 1; pc_id < info.n_pieces; pc_id++) {
        int frame1 = std::floor(
            ((info.piece_start[pc_id] - magic::SEARCHBUFFER) * magic::DOWNSAMPLE + info.piece_delay[pc_id]) /
                static_cast<float>(Nf / 2));
        int j = std::floor(
            (info.piece_end[pc_id - 1] - magic::SEARCHBUFFER) * magic::DOWNSAMPLE + info.piece_delay[pc_id - 1]) /
            static_cast<float>(Nf / 2);
        int delay_jump = info.piece_delay[pc_id] - info.piece_delay[pc_id - 1];

        frame1 = std::clamp(frame1, 0, j);

        if (delay_jump < -(int) (Nf / 2)) {
            int frame2 =
                (int) ((info.piece_start[pc_id] - magic::SEARCHBUFFER) * magic::DOWNSAMPLE +
                       std::abs(delay_jump)) / (Nf / 2) + 1;

            for (long frame = frame1; frame <= frame2; frame++) {
                if (frame < stop_frame) {
                    frame_was_skipped[frame] = true;

                    frame_disturbance[frame] = 0;
                    frame_disturbance_asym_add[frame] = 0;
                }
            }
        }
    }

    long nn = magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS + max_n_samples;

    auto tweaked_deg = Signal(nn);

    for (long i = 0; i < nn; i++) {
        tweaked_deg[i] = 0;
    }

    for (long i = magic::SEARCHBUFFER * magic::DOWNSAMPLE; i < nn - magic::SEARCHBUFFER * magic::DOWNSAMPLE; i++) {
        long pc_id = info.n_pieces - 1;
        long delay;

        while ((pc_id >= 0) && (info.piece_start[pc_id] * magic::DOWNSAMPLE > i)) {
            pc_id--;
        }
        if (pc_id >= 0) {
            delay = info.piece_delay[pc_id];
        } else {
            delay = info.piece_delay[0];
        }

        long j = i + delay;
        if (j < magic::SEARCHBUFFER * magic::DOWNSAMPLE) {
            j = magic::SEARCHBUFFER * magic::DOWNSAMPLE;
        }
        if (j >= static_cast<long>(nn - magic::SEARCHBUFFER * magic::DOWNSAMPLE)) {
            j = nn - magic::SEARCHBUFFER * magic::DOWNSAMPLE - 1;
        }
        tweaked_deg[i] = info.rec.data[static_cast<long>(j)];
    }

    if (there_is_a_bad_frame) {
        for (long frame = 0; frame <= stop_frame; frame++) {
            frame_is_bad[frame] = (frame_disturbance[frame] > magic::THRESHOLD_BAD_FRAMES);

            smeared_frame_is_bad[frame] = false;
        }
        frame_is_bad[0] = false;

        for (long frame = magic::SMEAR_RANGE; frame < stop_frame - magic::SMEAR_RANGE; frame++) {
            long max_itself_and_left = frame_is_bad[frame];
            long max_itself_and_right = frame_is_bad[frame];
            long mini;

            for (long i = -magic::SMEAR_RANGE; i <= 0; i++) {
                if (max_itself_and_left < frame_is_bad[frame + i]) {
                    max_itself_and_left = frame_is_bad[frame + i];
                }
            }

            for (long i = 0; i <= magic::SMEAR_RANGE; i++) {
                if (max_itself_and_right < frame_is_bad[frame + i]) {
                    max_itself_and_right = frame_is_bad[frame + i];
                }
            }

            mini = max_itself_and_left;
            if (mini > max_itself_and_right) {
                mini = max_itself_and_right;
            }

            smeared_frame_is_bad[frame] = mini;
        }

        number_of_bad_intervals = 0;
        {
            long frame = 0;
            while (frame <= stop_frame) {

                while ((frame <= stop_frame) && (!smeared_frame_is_bad[frame])) {
                    frame++;
                }

                if (frame <= stop_frame) {
                    start_frame_of_bad_interval[number_of_bad_intervals] = frame;

                    while ((frame <= stop_frame) && (smeared_frame_is_bad[frame])) {
                        frame++;
                    }

                    if (frame <= stop_frame) {
                        stop_frame_of_bad_interval[number_of_bad_intervals] = frame;

                        if (stop_frame_of_bad_interval[number_of_bad_intervals] -
                            start_frame_of_bad_interval[number_of_bad_intervals] >=
                            magic::MINIMUM_NUMBER_OF_BAD_FRAMES_IN_BAD_INTERVAL) {
                            number_of_bad_intervals++;
                        }
                    }
                }
            }
        }

        for (bad_interval = 0; bad_interval < number_of_bad_intervals; bad_interval++) {
            start_sample_of_bad_interval[bad_interval] =
                start_frame_of_bad_interval[bad_interval] * (Nf / 2) + magic::SEARCHBUFFER * magic::DOWNSAMPLE;
            stop_sample_of_bad_interval[bad_interval] =
                stop_frame_of_bad_interval[bad_interval] * (Nf / 2) + Nf + magic::SEARCHBUFFER * magic::DOWNSAMPLE;
            if (stop_frame_of_bad_interval[bad_interval] > stop_frame) {
                stop_frame_of_bad_interval[bad_interval] = stop_frame;
            }

            number_of_samples_in_bad_interval[bad_interval] =
                stop_sample_of_bad_interval[bad_interval] - start_sample_of_bad_interval[bad_interval];
        }

        search_range_in_samples = magic::SEARCH_RANGE_IN_TRANSFORM_LENGTH * Nf;

        for (bad_interval = 0; bad_interval < number_of_bad_intervals; bad_interval++) {
            auto ref = Signal(2 * search_range_in_samples + number_of_samples_in_bad_interval[bad_interval]);
            auto deg = Signal(2 * search_range_in_samples + number_of_samples_in_bad_interval[bad_interval]);
            float best_correlation;
            int delay_in_samples;

            for (long i = 0; i < search_range_in_samples; i++) {
                ref[i] = 0.0f;
            }
            for (long i = 0; i < number_of_samples_in_bad_interval[bad_interval]; i++) {
                ref[search_range_in_samples + i] = info.src.data[start_sample_of_bad_interval[bad_interval] + i];
            }
            for (long i = 0; i < search_range_in_samples; i++) {
                ref[search_range_in_samples + number_of_samples_in_bad_interval[bad_interval] + i] = 0.0f;
            }

            for (long i = 0;
                 i < 2 * search_range_in_samples + number_of_samples_in_bad_interval[bad_interval];
                 i++) {

                int j = start_sample_of_bad_interval[bad_interval] - search_range_in_samples + i;
                int nn = max_n_samples - magic::SEARCHBUFFER * magic::DOWNSAMPLE +
                         magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS;

                if (j < magic::SEARCHBUFFER * magic::DOWNSAMPLE) {
                    j = magic::SEARCHBUFFER * magic::DOWNSAMPLE;
                }
                if (j >= nn) {
                    j = nn - 1;
                }
                deg[i] = tweaked_deg[j];
            }

            delay_in_samples = compute_delay(0,
                                             2 * search_range_in_samples +
                                             number_of_samples_in_bad_interval[bad_interval],
                                             search_range_in_samples,
                                             ref,
                                             deg,
                                             &best_correlation);

            delay_in_samples_in_bad_interval[bad_interval] = delay_in_samples;

            if (best_correlation < 0.5) {
                delay_in_samples_in_bad_interval[bad_interval] = 0;
            }
        }

        if (number_of_bad_intervals > 0) {
            auto doubly_tweaked_deg = Signal(max_n_samples + magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS);

            for (long i = 0; i < max_n_samples + magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS; i++) {
                doubly_tweaked_deg[i] = tweaked_deg[i];
            }

            for (bad_interval = 0; bad_interval < number_of_bad_intervals; bad_interval++) {
                int delay = delay_in_samples_in_bad_interval[bad_interval];

                for (int i = start_sample_of_bad_interval[bad_interval];
                     i < stop_sample_of_bad_interval[bad_interval]; i++) {
                    int j = std::clamp(static_cast<int>(i + delay), 0, static_cast<int>(max_n_samples - 1));
                    doubly_tweaked_deg[i] = tweaked_deg[j];
                }
            }

            auto untweaked_deg = info.rec.data;
            info.rec.data = doubly_tweaked_deg;

            for (bad_interval = 0; bad_interval < number_of_bad_intervals; bad_interval++) {

                for (long frame = start_frame_of_bad_interval[bad_interval];
                     frame < stop_frame_of_bad_interval[bad_interval];
                     frame++) {

                    int start_sample_ref = magic::SEARCHBUFFER * magic::DOWNSAMPLE + frame * Nf / 2;
                    int start_sample_deg = start_sample_ref;

                    short_term_fft(fft_ctx, Nf, info.rec, w_hanning, start_sample_deg, hz_spectrum_deg, fft_tmp);

                    freq_warping(hz_spectrum_deg, Nb, pitch_pow_dens_deg, frame);
                }

                oldScale = 1;
                for (long frame = start_frame_of_bad_interval[bad_interval];
                     frame < stop_frame_of_bad_interval[bad_interval];
                     frame++) {
                    int band;

                    float total_audible_pow_ref = total_audible(frame, pitch_pow_dens_ref, 1);
                    float total_audible_pow_deg = total_audible(frame, pitch_pow_dens_deg, 1);

                    scale = (total_audible_pow_ref + (float) 5E3) / (total_audible_pow_deg + (float) 5E3);

                    if (frame > 0) {
                        scale = (float) 0.2 * oldScale + (float) 0.8 * scale;
                    }
                    oldScale = scale;

                    if (scale > (float) magic::MAX_SCALE) scale = (float) magic::MAX_SCALE;

                    if (scale < (float) magic::MIN_SCALE) {
                        scale = (float) magic::MIN_SCALE;
                    }

                    for (band = 0; band < Nb; band++) {
                        pitch_pow_dens_deg[frame * Nb + band] *= scale;
                    }

                    intensity_warping_of(loudness_dens_ref, frame, pitch_pow_dens_ref);
                    intensity_warping_of(loudness_dens_deg, frame, pitch_pow_dens_deg);

                    for (band = 0; band < Nb; band++) {
                        disturbance_dens[band] = loudness_dens_deg[band] - loudness_dens_ref[band];
                    }

                    for (band = 0; band < Nb; band++) {
                        deadzone[band] = std::min(loudness_dens_deg[band], loudness_dens_ref[band]);
                        deadzone[band] *= 0.25;
                    }

                    for (band = 0; band < Nb; band++) {
                        float d = disturbance_dens[band];
                        float m = deadzone[band];

                        if (d > m) {
                            disturbance_dens[band] -= m;
                        } else {
                            if (d < -m) {
                                disturbance_dens[band] += m;
                            } else {
                                disturbance_dens[band] = 0;
                            }
                        }
                    }

                    frame_disturbance[frame] = std::min(frame_disturbance[frame],
                                                        pseudo_Lp(disturbance_dens, magic::D_POW_F));

                    multiply_with_asymmetry_factor(disturbance_dens, frame, pitch_pow_dens_ref, pitch_pow_dens_deg);

                    frame_disturbance_asym_add[frame] = std::min(frame_disturbance_asym_add[frame],
                                                                 pseudo_Lp(disturbance_dens, magic::A_POW_F));
                }
            }
            info.rec.data = untweaked_deg;
        }
    }


    for (long frame = 0; frame <= stop_frame; frame++) {
        float h = 1;

        if (stop_frame + 1 > 1000) {
            long n = (max_n_samples - 2 * magic::SEARCHBUFFER * magic::DOWNSAMPLE) / (Nf / 2) - 1;
            double timeWeightFactor = (n - (float) 1000) / (float) 5500;
            if (timeWeightFactor > (float) 0.5) timeWeightFactor = (float) 0.5;
            h = (float) (((float) 1.0 - timeWeightFactor) + timeWeightFactor * (float) frame / (float) n);
        }

        time_weight[frame] = h;
    }

    for (long frame = 0; frame <= stop_frame; frame++) {

        float h = std::pow((total_power_ref[frame] + 1E5) / 1E7, 0.04);

        frame_disturbance[frame] /= h;
        frame_disturbance_asym_add[frame] /= h;

        if (frame_disturbance[frame] > 45) {
            frame_disturbance[frame] = 45;
        }
        if (frame_disturbance_asym_add[frame] > 45) {
            frame_disturbance_asym_add[frame] = 45;
        }
    }

    d_indicator = Lpq_weight(start_frame, stop_frame, magic::D_POW_S, magic::D_POW_T, frame_disturbance, time_weight);
    a_indicator = Lpq_weight(start_frame, stop_frame, magic::A_POW_S, magic::A_POW_T, frame_disturbance_asym_add,
                             time_weight);

    info.rate = (float) (4.5 - magic::D_WEIGHT * d_indicator - magic::A_WEIGHT * a_indicator);
}

}
