#include <iostream>
#include <iomanip>

#include "rating/measure.hpp"
#include "avio.hpp"

namespace tgvoipcontest {

void init_signal_info(const std::vector<float>& data, SignalInfo& info) {
    const size_t len = data.size();
    info.data = Signal(data.begin(), data.end(), len + magic::DATAPADDING_MS * magic::SAMPLE_RATE_MS);
    info.VAD = Signal(len / magic::DOWNSAMPLE);
    info.logVAD = Signal(len / magic::DOWNSAMPLE);
    info.n_samples = len;
}

float compute_rate(const std::vector<float>& source, const std::vector<float>& recorded) {
    RatingContext ctx;
    init_signal_info(source, ctx.src);
    init_signal_info(recorded, ctx.rec);

    measure_rate(ctx);
    return std::clamp(ctx.rate + 0.5f, 1.0f, 5.0f);
}

std::vector<float> downsample(const std::vector<float>& src) {
    using tgvoipcontest::Resampler;
    using tgvoipcontest::SamplingParams;

    Resampler<float, false, float, false> resampler{
        SamplingParams{1, AV_SAMPLE_FMT_FLT, 48000},
        SamplingParams{1, AV_SAMPLE_FMT_FLT, tgvoipcontest::magic::SAMPLE_RATE}
    };

    resampler.write_interleaved(src);
    resampler.finalize();
    return resampler.read_interleaved();
}

}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage:\n"
                     "tgvoiprate source_sound recorded_sound" << std::endl;
        return 1;
    }

    auto data1 = tgvoipcontest::downsample(tgvoipcontest::OpusReader::read_all_samples(argv[1]));
    auto data2 = tgvoipcontest::downsample(tgvoipcontest::OpusReader::read_all_samples(argv[2]));

    float res = tgvoipcontest::compute_rate(data1, data2);
    std::cout << std::setprecision(4) << res << std::endl;

    return 0;
}
