#include <string>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <cstring>
#include <algorithm>

#include "processing.hpp"
#include "dsp.hpp"


namespace tgvoipcontest {

RatingModelException::RatingModelException(const std::string& arg)
    : runtime_error(arg) {}

static double align_filter_dB[26][2] = {{0.,    -500},
                                        {50.,   -500},
                                        {100.,  -500},
                                        {125.,  -500},
                                        {160.,  -500},
                                        {200.,  -500},
                                        {250.,  -500},
                                        {300.,  -500},
                                        {350.,  0},
                                        {400.,  0},
                                        {500.,  0},
                                        {600.,  0},
                                        {630.,  0},
                                        {800.,  0},
                                        {1000., 0},
                                        {1250., 0},
                                        {1600., 0},
                                        {2000., 0},
                                        {2500., 0},
                                        {3000., 0},
                                        {3250., 0},
                                        {3500., -500},
                                        {4000., -500},
                                        {5000., -500},
                                        {6300., -500},
                                        {8000., -500}};


static double standard_IRS_filter_dB[26][2] = {{0.,    -200},
                                               {50.,   -40},
                                               {100.,  -20},
                                               {125.,  -12},
                                               {160.,  -6},
                                               {200.,  0},
                                               {250.,  4},
                                               {300.,  6},
                                               {350.,  8},
                                               {400.,  10},
                                               {500.,  11},
                                               {600.,  12},
                                               {700.,  12},
                                               {800.,  12},
                                               {1000., 12},
                                               {1300., 12},
                                               {1600., 12},
                                               {2000., 12},
                                               {2500., 12},
                                               {3000., 12},
                                               {3250., 12},
                                               {3500., 4},
                                               {4000., -200},
                                               {5000., -200},
                                               {6300., -200},
                                               {8000., -200}};


static constexpr double TARGET_AVG_POWER = 1E7;

static Signal alloc_other(const RatingContext& ctx) {
    return Signal(std::max(
        std::max(
            ctx.src.n_samples + magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS,
            ctx.rec.n_samples + magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS
        ),
        12 * magic::Align_Nfft
    ));
}

static void fix_power_level(SignalInfo& info, long max_n_samples) {
    long n = info.n_samples;
    auto align_filtered = info.data.copy();

    apply_filter(align_filtered, 26, align_filter_dB);

    auto power_above_300Hz = (float) pow_of(
        align_filtered,
        magic::SEARCHBUFFER * magic::DOWNSAMPLE,
        n - magic::SEARCHBUFFER * magic::DOWNSAMPLE + magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS,
        max_n_samples - 2 * magic::SEARCHBUFFER * magic::DOWNSAMPLE + magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS
    );

    float global_scale = std::sqrt(TARGET_AVG_POWER / power_above_300Hz);

    for (long i = 0; i < n; i++) {
        info.data[i] *= global_scale;
    }
}


void measure_rate(RatingContext& ctx) {
    if (((ctx.src.n_samples - 2 * magic::SEARCHBUFFER * magic::DOWNSAMPLE < magic::SAMPLE_RATE / 4) ||
         (ctx.rec.n_samples - 2 * magic::SEARCHBUFFER * magic::DOWNSAMPLE < magic::SAMPLE_RATE / 4))) {
        throw RatingModelException{"Reference or Degraded below 1/4 second"};
    }

    int maxNsamples = std::max(ctx.src.n_samples, ctx.rec.n_samples);

    // level normalization
    fix_power_level(ctx.src, maxNsamples);
    fix_power_level(ctx.rec, maxNsamples);

    // IRS filtering
    apply_filter(ctx.src.data, 26, standard_IRS_filter_dB);
    apply_filter(ctx.rec.data, 26, standard_IRS_filter_dB);

    auto model_ref = ctx.src.data.copy();
    auto model_deg = ctx.rec.data.copy();

    // input filtering
    input_filter(ctx.src);
    input_filter(ctx.rec);

    // Variable delay compensation
    calc_VAD(ctx.src);
    calc_VAD(ctx.rec);

    auto ftmp = alloc_other(ctx);
    crude_align(ctx, magic::WHOLE_SIGNAL, ftmp);
    pieces_locate(ctx, ftmp);

    ctx.src.data = model_ref;
    ctx.rec.data = model_deg;

    if (ctx.src.n_samples < ctx.rec.n_samples) {
        auto new_ref = ctx.src.data.copy(ctx.rec.n_samples + magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS);
        ctx.src.data = new_ref;
    } else if (ctx.src.n_samples > ctx.rec.n_samples) {
        auto new_deg = ctx.rec.data.copy(ctx.src.n_samples + magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS);
        ctx.rec.data = new_deg;
    }

    voip_qos_model(ctx);
}

}
