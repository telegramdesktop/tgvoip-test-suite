#include <utility>



#pragma once

#include <tgvoip/audio/AudioIO.h>
#include <tgvoip/threading.h>
#include <tgvoip/OpusDecoder.h>
#include <tgvoip/OpusEncoder.h>
#include <tgvoip/json11.hpp>
#include <tgvoip/VoIPServerConfig.h>

#include "avio.hpp"



namespace tgvoipcontest {

void set_voip_config(const json11::Json& config) {
    tgvoip::ServerConfig::SetSharedInstance(new tgvoip::ServerConfig{config});
}

class OpusAudioInput : public tgvoip::audio::AudioInput {
public:
    using NoMoreDataCallback = std::function<void(void)>;
    NoMoreDataCallback no_more_data_callback;

private:
    tgvoip::Thread worker;
    std::atomic_bool running{false};

    OpusReader reader;
    Resampler<float, false, int16_t, false> resampler;
    bool out_of_data = false;

public:
    explicit OpusAudioInput(const char* filename)
        : worker([this]() {
                     while (running.load(std::memory_order_relaxed))
                         loop();
                 }
    ), reader(filename), resampler(
        reader.get_sampling_parameters(),
        SamplingParams{1, AV_SAMPLE_FMT_S16, 48000}
    ) {}

    void Start() override {
        running.store(true, std::memory_order_relaxed);
        worker.Start();
    }

    void Stop() override {
        resampler.finalize();
    }

    void on_no_more_data(NoMoreDataCallback callback) {
        no_more_data_callback = std::move(callback);
    }

    ~OpusAudioInput() override {
        running.store(false, std::memory_order_relaxed);
        worker.Join();
    }

private:
    size_t got1 = 0;
    size_t got2 = 0;
    void loop() {
        double t = tgvoip::VoIPController::GetCurrentTime();

        std::vector<int16_t> data;
        bool request_finalization = false;

        if (!out_of_data) {
            try {
                auto raw_samples = reader.read_more();
                got1 += raw_samples.size();
                resampler.write_interleaved(raw_samples);
            } catch (OpusNoMoreData&) {
                resampler.finalize();
                out_of_data = true;
            }

            data = resampler.read_interleaved(960);

            if (data.size() % 960)
                std::cerr << "Warning: support not 960 divisible data length" << std::endl;
        } else {
            data = resampler.read_interleaved(960);
            if (data.empty()) {
                data.assign(960, 0);
                request_finalization = true;
            }
        }
        got2 += data.size();

        for (size_t i = 0; i < data.size() / 960; ++i)
            InvokeCallback(reinterpret_cast<unsigned char*>(data.data() + 960 * i), 960 * sizeof(int16_t));

        if (request_finalization && no_more_data_callback) {
            no_more_data_callback();
            no_more_data_callback = {};
        }

        double sl = 0.02 - (tgvoip::VoIPController::GetCurrentTime() - t);
        if (sl > 0.0)
            tgvoip::Thread::Sleep(sl);
    }
};


class OpusAudioOutput : public tgvoip::audio::AudioOutput {
private:
    bool playing{false};
    tgvoip::Thread worker;
    std::atomic_bool running{false};

    OpusMonoWriter writer;
    Resampler<int16_t, false, float, false> resampler;

public:
    explicit OpusAudioOutput(const char* filename)
        : worker([this]() {
        try {
            while (running.load(std::memory_order_relaxed))
                loop();
        } catch (OpusNoMoreData&) {}
    }), writer(filename), resampler(
        SamplingParams{1, AV_SAMPLE_FMT_S16, 48000},
        writer.get_sampling_parameters()
    ) {}

    void Start() override {
        playing = true;
        running.store(true, std::memory_order_relaxed);
        worker.Start();
    }

    void Stop() override {
        resampler.finalize();
        writer.write(resampler.read_interleaved());
        writer.finalize();
        playing = false;
    }

    bool IsPlaying() override {
        return playing;
    }

    ~OpusAudioOutput() override {
        running.store(false, std::memory_order_relaxed);
        worker.Join();
    }

private:
    void loop() {
        std::vector<int16_t> buf(960, 0);
        double t = tgvoip::VoIPController::GetCurrentTime();
        InvokeCallback(reinterpret_cast<unsigned char*>(buf.data()), buf.size() * sizeof(int16_t));
        resampler.write_interleaved(buf);
        writer.write(resampler.read_interleaved());
        double sl = 0.02 - (tgvoip::VoIPController::GetCurrentTime() - t);
        if (sl > 0.0)
            tgvoip::Thread::Sleep(sl);
    }
};


class OpusAudioIO : public tgvoip::audio::AudioIO {
public:
    using NoMoreDataCallback = OpusAudioInput::NoMoreDataCallback;

    std::shared_ptr<OpusAudioOutput> encoder;
    std::shared_ptr<OpusAudioInput> decoder;

    OpusAudioIO(const char* input_file, const char* output_file, NoMoreDataCallback no_more_callback) {
        encoder = std::make_shared<OpusAudioOutput>(output_file);
        decoder = std::make_shared<OpusAudioInput>(input_file);
        decoder->on_no_more_data(std::move(no_more_callback));
    }

    tgvoip::audio::AudioInput* GetInput() override {
        return decoder.get();
    }

    tgvoip::audio::AudioOutput* GetOutput() override {
        return encoder.get();
    }
};


}