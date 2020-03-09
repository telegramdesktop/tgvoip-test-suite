#include <complex>
#include <iomanip>
#include <iostream>
#include <opusfile.h>
#include <valarray>
#include <vector>

typedef std::complex<double> ComplexVal;
typedef std::valarray<ComplexVal> SampleArray;

void FFT(SampleArray& values) {
    const size_t N = values.size();
    if (N <= 1)
        return;

    SampleArray evens = values[std::slice(0, N / 2, 2)];
    SampleArray odds = values[std::slice(1, N / 2, 2)];

    FFT(evens);
    FFT(odds);

    for (size_t i = 0; i < N / 2; i++) {
        ComplexVal index = std::polar(1.0, -2 * M_PI * i / N) * odds[i];
        values[i] = evens[i] + index;
        values[i + N / 2] = evens[i] - index;
    }
}

class Estimator {
private:
    static const size_t frame_size = 1024;
    constexpr static const double silence_border = 1e-2;
    static double window[frame_size];
    OggOpusFile *ref;
    OggOpusFile *tst;
    float buf[frame_size];
    SampleArray fft;
    double spectre[frame_size / 2];
    size_t ref_frames;
    size_t ref_silence;
    double final_ref_spectre[frame_size / 2];
    size_t tst_frames;
    size_t tst_silence;
    double final_tst_spectre[frame_size / 2];

public:
    Estimator(const char *ref_file, const char *tst_file)
    : ref(nullptr)
    , tst(nullptr)
    , buf()
    , fft(frame_size)
    , spectre()
    , ref_frames(0)
    , ref_silence(0)
    , final_ref_spectre()
    , tst_frames(0)
    , tst_silence(0)
    , final_tst_spectre()
    {
        int err;
        ref = op_open_file(ref_file, &err);
        if (err) {
            close_files();
            throw std::invalid_argument("Can't open the reference file");
        }
        tst = op_open_file(tst_file, &err);
        if (err) {
            close_files();
            throw std::invalid_argument("Can't open the test file");
        }
    }

    ~Estimator() {
        close_files();
    }

    double evaluate() {
        evaluate_ref();
        evaluate_tst();

        double trail_ratio = (.0 + tst_frames - tst_silence) / (.0 + ref_frames - ref_silence);
        std::vector<double> spectre_eval;
        for (size_t i = 0; i < frame_size / 2 / 20; ++i)
            if (final_ref_spectre[i] >= final_tst_spectre[i])
                spectre_eval.push_back(final_tst_spectre[i] / final_ref_spectre[i]);
        std::sort(spectre_eval.begin(), spectre_eval.end());
        double spectre_est = spectre_eval.empty() ? 0 : spectre_eval[spectre_eval.size() / 2];
        double trail_est = std::pow(trail_ratio < 1 ? trail_ratio : 1 / trail_ratio, 3);

        double final_est = 1.5 * trail_est + 3.5 * spectre_est;
        final_est = std::min(5.0, std::max(1.0, final_est));
        return final_est;
    }

    static void init_hanning_window() {
        double frame_size_minus1 = frame_size - 1;
        for (size_t i = 0; i < frame_size; ++i)
            window[i] = 0.5 * (1 - cos (2. * M_PI * (i / frame_size_minus1)));
    }

private:
    void close_files() {
        if (ref) {
            op_free(ref);
            ref = nullptr;
        }
        if (tst) {
            op_free(tst);
            tst = nullptr;
        }
    }

    bool read_frame(OggOpusFile *decoder) {
        int read, remains = frame_size;
        while (remains > 0 && (read = op_read_float(decoder, buf + frame_size - remains, remains, nullptr)) > 0)
            remains -= read;
        return remains == 0;
    }

    void calc_fft() {
        for (size_t i = 0; i < frame_size; ++i)
            fft[i] = ComplexVal(buf[i], 0) * window[i];
        FFT(fft);
    }

    void calc_spectre() {
        calc_fft();
        for (size_t i = 0; i < frame_size / 2; ++i)
            spectre[i] = std::abs(fft[i]);
    }

    bool is_silence() {
        for (double amp : buf)
            if (std::abs(amp) > silence_border)
                return false;
        return true;
    }

    void evaluate_ref() {
        op_raw_seek(ref, 0);
        ref_frames = 0;
        ref_silence = 0;
        while (read_frame(ref)) {
            ++ref_frames;
            if (is_silence())
                ++ref_silence;
            calc_spectre();
            for (size_t i = 0; i < frame_size / 2; ++i)
                final_ref_spectre[i] += spectre[i];
        }
    }

    void evaluate_tst() {
        op_raw_seek(tst, 0);
        tst_frames = 0;
        tst_silence = 0;
        while (read_frame(tst)) {
            if (is_silence()) {
                if (tst_frames > ref_frames)
                    break;
                ++tst_silence;
            }
            ++tst_frames;
            calc_spectre();
            for (size_t i = 0; i < frame_size / 2; ++i)
                final_tst_spectre[i] += spectre[i];
        }
    }
};

double Estimator::window[] = {1};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: tgvoiprate reference.ogg test.ogg" << std::endl;
        return 1;
    }

    Estimator::init_hanning_window();
    try {
        Estimator estimator(argv[1], argv[2]);
        std::cout << estimator.evaluate() << std::endl;
    }
    catch (std::exception &err) {
        std::cerr << err.what() << std::endl;
        return 1;
    }

    return 0;
}