#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/avstring.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavresample/avresample.h>
#include <string.h>
}

#include <vector>
#include <sstream>
#include <iostream>
#include <cmath>



namespace tgvoipcontest {

class AudioIOException : public std::runtime_error {
public:
    explicit AudioIOException(const std::string& arg)
        : runtime_error(arg) {}
};


class DetailedAudioIOException : public AudioIOException {
public:
    DetailedAudioIOException(const std::string& arg, const int error)
        : AudioIOException(format(arg, error)) {}

private:
    static std::string format(const std::string& title, const int error) {
        char buf[256];
        av_strerror(error, buf, sizeof(buf));
        std::ostringstream msg;
        msg << title << ": " << buf;
        return msg.str();
    }
};


class OpusNoMoreData : public AudioIOException {
public:
    OpusNoMoreData()
        : AudioIOException("No more data") {}
};


template <class T>
static T* die_if_null(T* ptr, const char* message) {
    if (!ptr)
        throw AudioIOException{message};
    return ptr;
}


struct SamplingParams {
    int channels;
    AVSampleFormat format;
    int sample_rate;

    bool is_planar() const {
        return av_sample_fmt_is_planar(format);
    }

    int sample_size() const {
        return av_get_bytes_per_sample(format);
    }

    static SamplingParams from_frame(AVFrame* frame) {
        return SamplingParams{
            frame->channels,
            static_cast<AVSampleFormat>(frame->format),
            frame->sample_rate
        };
    }
};

bool operator !=(const SamplingParams& a, const SamplingParams& b) {
    return !(a.sample_rate == b.sample_rate && a.format == b.format && a.channels == b.channels);
}


template <class FromT, class ToT>
class LowLevelResampler {
public:
    virtual ~LowLevelResampler() noexcept {
        cleanup();
    }

private:
    AVAudioResampleContext* resample_context = nullptr;
    SamplingParams from_params = SamplingParams{};
    SamplingParams to_params = SamplingParams{};

protected:
    LowLevelResampler() {
        try {
            init_resampler();
        } catch (AudioIOException&) {
            cleanup();
            std::rethrow_exception(std::current_exception());
        }
    }


protected:
    const SamplingParams& get_from_params() const {
        return from_params;
    }

    const SamplingParams& get_to_params() const {
        return to_params;
    }

    size_t get_samples_available() {
        return avresample_available(resample_context);
    }

    virtual void finalize() {
        write_frame(nullptr);
    }

    void set_parameters(SamplingParams from, SamplingParams to) {
        if (from.sample_size() != sizeof(FromT) || to.sample_size() != sizeof(ToT))
            throw AudioIOException{"Bad resampling parameters"};

        from_params = from;
        to_params = to;

        avresample_close(resample_context);

        av_opt_set_int(resample_context, "in_channel_layout",
                       av_get_default_channel_layout(from.channels), 0);
        av_opt_set_int(resample_context, "out_channel_layout",
                       av_get_default_channel_layout(to.channels), 0);
        av_opt_set_int(resample_context, "in_sample_rate", from.sample_rate, 0);
        av_opt_set_int(resample_context, "out_sample_rate", to.sample_rate, 0);
        av_opt_set_int(resample_context, "in_sample_fmt", from.format, 0);
        av_opt_set_int(resample_context, "out_sample_fmt", to.format, 0);

        int error;
        if ((error = avresample_open(resample_context)) < 0) {
            throw DetailedAudioIOException{"Cannot open resample context", error};
        }
    }

    void write_frame(AVFrame* frame) {
        if (frame)
            if (frame->sample_rate != from_params.sample_rate
                || frame->format != from_params.format
                || frame->channels != from_params.channels) {
                throw AudioIOException{"Bad resampling parameters: invalid argument"};
            }

        int error;
        if ((error = avresample_convert_frame(resample_context, nullptr, frame)) < 0) {
            throw DetailedAudioIOException{"Cannot convert samples (1)", error};
        }
    }

    std::pair<uint8_t**, size_t> read_samples(const size_t max_samples) {
        if (max_samples == 0)
            return std::pair{nullptr, 0};

        uint8_t** samples = init_converted_samples(max_samples);

        int error = avresample_read(resample_context, samples, max_samples);
        if (error < 0) {
            free_converted_samples(samples);
            throw DetailedAudioIOException{"Cannot convert samples (2)", error};
        }

        return std::pair{samples, error};
    }

    size_t read_data_interleaved(ToT* buffer, const size_t length) {
        if (to_params.is_planar())
            throw AudioIOException{"Bad resampling parameters: the output is planar"};

        auto[samples, num_samples] = read_samples(length / to_params.channels);

        if (samples) {
            memcpy(buffer, samples[0], num_samples * sizeof(ToT) * to_params.channels);
            free_converted_samples(samples);
        }
        return num_samples * to_params.channels;
    }

    std::vector<ToT> read_available_data_interleaved(const size_t max_sampels = 0) {
        size_t samples_available = get_samples_available();
        if (max_sampels && max_sampels < samples_available)
            samples_available = max_sampels;

        int num_samples_total = samples_available * to_params.channels;
        std::vector<ToT> result(num_samples_total, ToT{});
        size_t got = read_data_interleaved(result.data(), result.size());
        result.resize(got);
        return result;
    }

    void write_data_interleaved(const FromT* data, const size_t len) {
        if (!len)
            return;

        if (len % from_params.channels)
            throw AudioIOException{"Bad length"};

        AVFrame* frame = init_frame(len / from_params.channels);
        memcpy(frame->data[0], data, len * sizeof(FromT));

        try {
            write_frame(frame);
        } catch (AudioIOException&) {
            free_frame(frame);
            std::rethrow_exception(std::current_exception());
        }

        free_frame(frame);
    }

    AVFrame* init_frame(const int num_samples) {
        AVFrame* frame;
        frame = die_if_null(av_frame_alloc(), "Cannot alloc frame");

        frame->format = from_params.format;
        frame->channel_layout = av_get_default_channel_layout(from_params.channels);
        frame->channels = from_params.channels;
        frame->sample_rate = from_params.sample_rate;

        if (num_samples) {
            frame->nb_samples = num_samples;

            int error = av_frame_get_buffer(frame, 0);
            if (error) {
                free_frame(frame);
                throw DetailedAudioIOException{"Cannot initialize frame", error};
            }
        }

        return frame;
    }

    void free_frame(AVFrame*& frame) {
        av_frame_free(&frame);
    }

    void free_converted_samples(uint8_t** samples) {
        if (samples) {
            av_freep(&samples[0]);
            av_freep(&samples);
        }
    }

private:
    void init_resampler() {
        resample_context = die_if_null(avresample_alloc_context(), "Cannot alloc resample context");
    }

    uint8_t** init_converted_samples(const int num_samples) {
        int error;
        uint8_t** samples;

        if ((error = av_samples_alloc_array_and_samples(&samples, NULL, to_params.channels,
                                                        num_samples, to_params.format, 0)) < 0) {
            throw DetailedAudioIOException{"Cannot alloc samples", error};
        }

        return samples;
    }

    void cleanup() {
        if (resample_context) {
            avresample_free(&resample_context);
        }
    }
};


class OpusReader : public LowLevelResampler<int16_t, float>, public LowLevelResampler<float, float> {
private:
    using IntResampler = LowLevelResampler<int16_t, float>;
    using FloatResampler = LowLevelResampler<float, float>;

    const AVCodec* codec = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVFormatContext* format_ctx = nullptr;

    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;

    bool input_finished = false;
    bool decoder_flushed = false;

    bool source_is_float = false;

public:
    explicit OpusReader(const char* filename) {
        av_register_all();
        avcodec_register_all();

        try {
            open_input_file(filename);
            init_resampler();
            packet = die_if_null(av_packet_alloc(), "Cannot alloc packet");
            frame = die_if_null(av_frame_alloc(), "Cannot alloc frame");
        } catch (AudioIOException&) {
            cleanup();
            std::rethrow_exception(std::current_exception());
        }
    }

    SamplingParams get_sampling_parameters() const {
        return {1, AV_SAMPLE_FMT_FLT, 48000};
    }

    std::vector<float> read_more() {
        std::vector<float> result;
        while (result.empty())
            result = read_some_more();
        return result;
    }

    ~OpusReader() noexcept override {
        cleanup();
    }

    static std::vector<float> read_all_samples(const char* filename) {
        OpusReader reader{filename};
        std::vector<float> result;
        try {
            while (true) {
                auto samples = reader.read_more();
                std::copy(samples.begin(), samples.end(), std::back_inserter(result));
            }
        } catch (OpusNoMoreData&) {}
        return result;
    }

private:
    void init_resampler() {
        AVCodecParameters* raw_params = format_ctx->streams[0]->codecpar;
        SamplingParams params{
            av_get_channel_layout_nb_channels(raw_params->channel_layout),
            static_cast<AVSampleFormat>(raw_params->format),
            raw_params->sample_rate
        };

        configure_resampler(params);
    }

    void configure_resampler(SamplingParams params) {
        switch (params.sample_size()) {
            case 4:
                source_is_float = true;
                FloatResampler::set_parameters(params, get_sampling_parameters());
                break;
            case 2:
                source_is_float = false;
                FloatResampler::set_parameters(params, get_sampling_parameters());
                break;
            default:
                throw AudioIOException{"Invalid sound format"};
        }
    }

    std::vector<float> read_some_more() {
        int error;

        if ((error = av_read_frame(format_ctx, packet)) < 0) {
            if (error == AVERROR_EOF) {
                input_finished = true;
                av_packet_unref(packet);
            } else {
                throw DetailedAudioIOException{"Cannot read packet", error};
            }
        }

        std::vector<float> result = decode();
        if (result.empty() && input_finished)
            throw OpusNoMoreData{};
        return result;
    }

    std::vector<float> decode() {
        int ret;

        if (!input_finished || !decoder_flushed) {
            if ((ret = avcodec_send_packet(codec_ctx, packet)) < 0) {
                throw DetailedAudioIOException{"Cannot decode packet", ret};
            }
            decoder_flushed = input_finished;
        }

        while (true) {
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                if (source_is_float) {
                    if (ret == AVERROR_EOF)
                        FloatResampler::finalize();
                    return FloatResampler::read_available_data_interleaved();
                } else {
                    if (ret == AVERROR_EOF)
                        IntResampler::finalize();
                    return IntResampler::read_available_data_interleaved();
                }
            }
            if (ret < 0) {
                throw DetailedAudioIOException{"Cannot decode packet", ret};
            }

            SamplingParams expected_params =
                source_is_float ? FloatResampler::get_from_params() : IntResampler::get_to_params();
            SamplingParams frame_params = SamplingParams::from_frame(frame);
            if (expected_params != frame_params) {
                std::cerr << "Change sampling parameters, "
                             "some samples may be lost" << std::endl;
                configure_resampler(frame_params);
            }

            if (source_is_float) {
                FloatResampler::write_frame(frame);
            } else {
                IntResampler::write_frame(frame);
            }
        }
    }

    void open_input_file(const char* filename) {
        int error;

        if ((error = avformat_open_input(&format_ctx, filename, nullptr, nullptr)) < 0) {
            throw DetailedAudioIOException{"Cannot open file (1)", error};
        }

        if ((error = avformat_find_stream_info(format_ctx, nullptr)) < 0) {
            throw DetailedAudioIOException{"Cannot open file (2)", error};
        }

        if (format_ctx->nb_streams != 1) {
            throw AudioIOException{"Too many streams in the file"};
        }

        codec = die_if_null(avcodec_find_decoder(format_ctx->streams[0]->codecpar->codec_id), "Cannot find codec");
        codec_ctx = die_if_null(avcodec_alloc_context3(codec), "Cannot alloc codec context");

        error = avcodec_parameters_to_context(codec_ctx, format_ctx->streams[0]->codecpar);
        if (error < 0) {
            throw DetailedAudioIOException{"Cannot retrieve codec parameters", error};
        }

        if ((error = avcodec_open2(codec_ctx, codec, nullptr)) < 0) {
            throw DetailedAudioIOException{"Cannot initialize decoder", error};
        }
    }

    void cleanup() noexcept {
        if (codec_ctx) {
            avcodec_free_context(&codec_ctx);
        }
        if (format_ctx) {
            avformat_close_input(&format_ctx);
            avformat_free_context(format_ctx);
        }
        if (packet) {
            av_packet_free(&packet);
        }
        if (frame) {
            av_frame_free(&frame);
        }
    }
};


class OpusMonoWriter {
private:
    AVCodecContext* codec_context = nullptr;
    AVFormatContext* format_context = nullptr;
    AVIOContext* io_context = nullptr;

    AVStream* stream = nullptr;
    const AVCodec* codec = nullptr;

    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    int64_t pts = 0;

    bool finalized = false;
    std::vector<float> data;

public:
    explicit OpusMonoWriter(const char* filename) {
        avcodec_register_all();
        av_register_all();

        try {
            open_output_file(filename);

            packet = die_if_null(av_packet_alloc(), "Cannot alloc frame");
            init_frame();
        } catch (AudioIOException&) {
            cleanup();
            std::rethrow_exception(std::current_exception());
        }
    }

    SamplingParams get_sampling_parameters() const {
        return {1, AV_SAMPLE_FMT_FLT, 48000};
    }

    void write(const std::vector<float>& samples) {
        if (finalized)
            throw OpusNoMoreData{};

        std::copy(samples.begin(), samples.end(), std::back_inserter(data));

        std::exception_ptr error;

        if (data.size() >= static_cast<size_t>(frame->nb_samples)) {
            const size_t available = data.size() / frame->nb_samples;
            const float* const raw_data = data.data();
            size_t put;

            try {
                for (put = 0; put < available; ++put) {
                    perform_write(raw_data + put * frame->nb_samples, frame->nb_samples);
                }
            } catch (AudioIOException&) {
                error = std::current_exception();
            }

            data.erase(data.begin(), data.begin() + put * frame->nb_samples);
        }

        if (error)
            std::rethrow_exception(error);
    }

    void finalize() {
        if (finalized)
            return;

        flush();
        finalized = true;

        while (encode_audio_frame(true));

        int error;
        if ((error = av_write_trailer(format_context)) < 0) {
            throw DetailedAudioIOException{"Cannot finalize stream", error};
        }
    }

    ~OpusMonoWriter() noexcept {
        try {
            finalize();
        } catch (AudioIOException&) {}
        cleanup();
    }

private:
    void flush() {
        if (!data.empty()) {
            write(std::vector<float>(frame->nb_samples - data.size(), 0.0f));
        }
    }

    void perform_write(const float* samples, const size_t len) {
        if (len != static_cast<size_t>(frame->nb_samples))
            throw AudioIOException{"Internal error"};

        int error;
        if ((error = av_frame_make_writable(frame)) < 0) {
            throw DetailedAudioIOException{"Cannot encode packet", error};
        }

        memcpy(frame->data[0], samples, len * sizeof(*samples));

        encode_audio_frame();
    }

    void init_frame() {
        frame = die_if_null(av_frame_alloc(), "Cannot alloc frame");
        frame->nb_samples = codec_context->frame_size;
        frame->format = codec_context->sample_fmt;
        frame->channel_layout = codec_context->channel_layout;
        frame->channels = codec_context->channels;

        int error;
        if ((error = av_frame_get_buffer(frame, 0)) < 0) {
            throw DetailedAudioIOException{"Cannot initialize frame", error};
        }
    }

    void open_output_file(const char* filename) {
        int error;

        if ((error = avio_open(&io_context, filename, AVIO_FLAG_WRITE)) < 0) {
            throw DetailedAudioIOException{"Cannot open file", error};
        }

        format_context = die_if_null(avformat_alloc_context(), "Could not alloc format context");
        format_context->pb = io_context;

        if (!(format_context->oformat = av_guess_format(nullptr, filename, nullptr))
            || !(format_context->oformat = av_guess_format("ogg", nullptr, nullptr))) {
            throw AudioIOException{"Cannot recognize output file format"};
        }
        av_strlcpy(format_context->filename, filename, sizeof(format_context->filename));

        codec = die_if_null(avcodec_find_encoder(AV_CODEC_ID_OPUS), "Could not find an Opus encoder");
        stream = die_if_null(avformat_new_stream(format_context, codec), "Cannot init stream");

        codec_context = die_if_null(avcodec_alloc_context3(codec), "Could not alloc codec context");

        setup_codec_params();

        if (format_context->oformat->flags & AVFMT_GLOBALHEADER)
            codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if ((error = avcodec_open2(codec_context, codec, nullptr)) < 0) {
            throw DetailedAudioIOException{"Cannot initialize encoder", error};
        }

        error = avcodec_parameters_from_context(stream->codecpar, codec_context);
        if (error < 0) {
            throw DetailedAudioIOException{"Cannot copy codec parameters", error};
        }

        if ((error = avformat_write_header(format_context, nullptr)) < 0) {
            throw DetailedAudioIOException{"Cannot write header", error};
        }
    }

    void setup_codec_params() {
        codec_context->sample_fmt = AV_SAMPLE_FMT_FLTP;
        codec_context->bit_rate = 64000;
        codec_context->channel_layout = AV_CH_LAYOUT_MONO;
        codec_context->channels = 1;
        codec_context->sample_rate = 48000;
    }

    bool encode_audio_frame(bool finish = false) {
        int error;

        if (!finish) {
            // TODO check if it is ok [units]
            frame->pts = pts;
            pts += frame->nb_samples;
        }

        error = avcodec_send_frame(codec_context, finish ? nullptr : frame);
        if (error < 0 && error != AVERROR_EOF) {
            throw DetailedAudioIOException{"Cannot encode packet", error};
        }

        error = avcodec_receive_packet(codec_context, packet);
        if (error == AVERROR(EAGAIN)) {
            return true;
        } else if (error == AVERROR_EOF) {
            return false;
        } else if (error < 0) {
            throw DetailedAudioIOException{"Cannot encode packet", error};
        }

        if ((error = av_write_frame(format_context, packet)) < 0) {
            throw DetailedAudioIOException{"Cannot write packet", error};
        }
        return true;
    }

    void cleanup() noexcept {
        if (codec_context) {
            avcodec_free_context(&codec_context);
        }
        if (io_context) {
            avio_close(io_context);
        }
        if (format_context) {
            avformat_free_context(format_context);
        }
        if (frame) {
            av_frame_free(&frame);
        }
        if (packet) {
            av_packet_free(&packet);
        }
    }
};


template <class FromT, bool from_planar, class ToT, bool to_planar>
class Resampler : public LowLevelResampler<FromT, ToT> {
private:
    using super = LowLevelResampler<FromT, ToT>;

public:
    Resampler(SamplingParams from, SamplingParams to) {
        if (from_planar != from.is_planar() || to_planar != to.is_planar())
            throw AudioIOException{"Bad resampling parameters"};

        super::set_parameters(from, to);
    }

    void finalize() override {
        super::finalize();
    }

    void write_interleaved(const std::vector<FromT>& data) {
        if (from_planar)
            throw AudioIOException{"Bad resampling parameters"};
        super::write_data_interleaved(data.data(), data.size());
    }

    std::vector<ToT> read_interleaved(const size_t max_samples = 0) {
        if (to_planar)
            throw AudioIOException{"Bad resampling parameters"};
        return super::read_available_data_interleaved(max_samples);
    }

    void write_planar(const std::vector<std::vector<FromT>>& data) {
        if (!from_planar)
            throw AudioIOException{"Bad resampling parameters"};

        if (data.size() != super::get_from_params().channels)
            throw AudioIOException{"Wrong number of channels"};

        for (const auto& channel : data)
            if (channel.size() != data.front().size())
                throw AudioIOException{"Channels must have equal sizes"};

        int num_samples = data.front().size();

        AVFrame* frame = super::init_frame(num_samples);
        for (int i = 0; i < super::get_from_params().channels; ++i)
            memcpy(frame->data[i], data[i].data(), num_samples * sizeof(FromT));

        try {
            super::write_frame(frame);
        } catch (AudioIOException&) {
            super::free_frame(frame);
            std::rethrow_exception(std::current_exception());
        }

        super::free_frame(frame);
    }

    std::vector<std::vector<ToT>> read_planar() {
        if (!to_planar)
            throw AudioIOException{"Bad resampling parameters"};

        size_t num_samples = super::get_samples_available();
        int num_channes = super::get_to_params().channels;

        auto[samples, got_samples] = super::read_samples(num_samples);
        std::vector<std::vector<ToT>> result(num_channes, std::vector<ToT>(got_samples, ToT{}));

        if (samples) {
            for (int i = 0; i < num_channes; ++i)
                memcpy(result[i].data(), samples[i], got_samples * sizeof(ToT));
            super::free_converted_samples(samples);
        }

        return result;
    }
};

}
