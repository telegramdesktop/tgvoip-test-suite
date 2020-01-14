#include <complex>
#include <iomanip>
#include <iostream>
#include <valarray>
#include <vector>
#include <fstream>

typedef std::complex<double> ComplexVal;

void FFT(std::valarray<ComplexVal>& values) {
    const size_t N = values.size();
    if (N <= 1)
        return;

    std::valarray<ComplexVal> evens = values[std::slice(0, N / 2, 2)];
    std::valarray<ComplexVal> odds = values[std::slice(1, N / 2, 2)];

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
    std::fstream ref;
    std::fstream tst;
    const size_t frame_size;
    const size_t spectre_size;
    std::valarray<double> window;
    float *frame;
    int16_t *iframe;
    std::valarray<ComplexVal> fft;
    std::valarray<double> spectre;
    size_t ref_frames;
    size_t ref_silence;
    std::valarray<double> final_ref_spectre;
    size_t tst_frames;
    size_t tst_silence;
    std::valarray<double> final_tst_spectre;
    unsigned char spectre_part;
    float trail_k;
    float spectre_k;
    float trail_pow;
    float noice_ratio;
    float loud_threshold;
    float multiple_threshold;

public:
    Estimator(const char *ref_file, const char *tst_file,
              unsigned char frame_size_pow=10, unsigned char spectre_part=30,
              float trail_k=2, float spectre_k=3,
              float trail_pow=2, float noice_ratio=10,
              float loud_threshold=5, float multiple_threshold=0.015)
    : ref(ref_file, std::ios::in | std::ios::binary)
    , tst(tst_file, std::ios::in | std::ios::binary)
    , frame_size(pow(2, frame_size_pow))
    , spectre_size(frame_size / 2)
    , window(frame_size)
    , frame(new float[frame_size])
    , iframe(new int16_t[frame_size])
    , fft(frame_size)
    , spectre(frame_size / 2)
    , ref_frames(0)
    , ref_silence(0)
    , final_ref_spectre(frame_size / 2)
    , tst_frames(0)
    , tst_silence(0)
    , final_tst_spectre(frame_size / 2)
    , spectre_part(spectre_part)
    , trail_k(trail_k)
    , spectre_k(spectre_k)
    , trail_pow(trail_pow)
    , noice_ratio(noice_ratio)
    , loud_threshold(loud_threshold)
    , multiple_threshold(multiple_threshold)
    {
        if (!ref_file) {
            close_files();
            throw std::invalid_argument("Can't open the reference file");
        }
        if (!tst_file) {
            close_files();
            throw std::invalid_argument("Can't open the test file");
        }
        init_hanning_window();
    }

    ~Estimator() {
        delete[] frame;
        delete[] iframe;
        close_files();
    }

    double calc_score() {
        double trail_ratio = (.0 + tst_frames - tst_silence) / (.0 + ref_frames - ref_silence);
        std::vector<double> spectre_eval;
        for (size_t i = 0; i < spectre_size / spectre_part; ++i)
            if (final_ref_spectre[i] >= final_tst_spectre[i])
                spectre_eval.push_back(final_tst_spectre[i] / final_ref_spectre[i]);
        double spectre_est = calc_median(spectre_eval.begin(), spectre_eval.end());
        double trail_est = std::pow(trail_ratio < 1 ? trail_ratio : 1 / trail_ratio, trail_pow);

        double final_est = trail_k * trail_est + spectre_k * spectre_est;
        final_est = std::min(5.0, std::max(1.0, final_est));
        return final_est;
    }

    double evaluate() {
        evaluate_ref();
        evaluate_tst();
        return calc_score();
    }

    void init_hanning_window() {
        double frame_size_minus1 = static_cast<double>(frame_size) - 1;
        for (size_t i = 0; i < frame_size; ++i)
            window[i] = 0.5 * (1 - cos (2. * M_PI * (i / frame_size_minus1)));
    }

private:
    void close_files() {
        if (ref.is_open())
            ref.close();
        if (tst.is_open())
            tst.close();
    }

    void to_float_frame() {
        int16_t *s = iframe + frame_size;
        float *d = frame + frame_size;
        while (s >= iframe && d >= frame) {
            *d = static_cast<float>(*s / 32768.0);
            if (*d > 1)
                *d = 1;
            if (*d < -1)
                *d = -1;
            --s;
            --d;
        }
    }

    bool read_frame(std::fstream &file) {
        file.read((char *) iframe, frame_size * sizeof(int16_t));
        to_float_frame();
        return !file.fail();
    }

    void calc_fft() {
        for (size_t i = 0; i < frame_size; ++i)
            fft[i] = ComplexVal(frame[i], 0) * window[i];
        FFT(fft);
    }

    void calc_spectre() {
        calc_fft();
        for (size_t i = 0; i < frame_size / 2; ++i)
            spectre[i] = std::abs(fft[i]);
    }

    static double calc_median(const std::valarray<double> &array) {
        size_t size = array.size();
        if (size == 0)
            return 0;
        std::vector<double> copy;
        for (const double val : array)
            copy.push_back(val);
        return calc_median(copy.begin(), copy.end());
    }

    template <typename Iterator>
    static double calc_median(Iterator first, Iterator last) {
        size_t size = std::distance(first, last);
        if (size == 0)
            return 0;
        std::sort(first, last);
        if (size % 2 == 0)
            return (*(first + size / 2 - 1) + *(first + size / 2)) / 2;
        return *(first + size / 2);
    }

    bool is_silence() {
        double median = calc_median(spectre);
        double max_spectre = spectre.max();
        bool is_loud = max_spectre > loud_threshold;
        bool is_multiple = median > multiple_threshold;
        bool speech = is_multiple or is_loud;
        bool noice = std::abs(median) > 1e-5 ? (max_spectre / median) < noice_ratio : false;
        bool good = speech and not noice;
        return not good;
    }

    void evaluate_ref() {
        ref_frames = 0;
        ref_silence = 0;
        final_ref_spectre = 0;
        ref.seekg(0, std::fstream::beg);
        while (read_frame(ref)) {
            calc_spectre();
            if (is_silence())
                ++ref_silence;
            ++ref_frames;
            final_ref_spectre += spectre;
        }
    }

    void evaluate_tst() {
        tst_frames = 0;
        tst_silence = 0;
        final_tst_spectre = 0;
        tst.seekg(0, std::fstream::beg);
        while (read_frame(tst)) {
            calc_spectre();
            if (is_silence()) {
                if (tst_frames > ref_frames)
                    break;
                ++tst_silence;
            }
            ++tst_frames;
            final_tst_spectre += spectre;
        }
    }
};

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: tgvoiprate reference.pcm [preprocessed.pcm] result.pcm" << std::endl;
        return 1;
    }

    try {
        if (argc == 3) {
            Estimator estimator(argv[1], argv[2]);
            std::cout << estimator.evaluate() << std::endl;
        } else {
            Estimator estimator_preproc(argv[1], argv[2], 9, 2, 0, 5);
            Estimator estimator_network(argv[2], argv[3]);
            std::cout << estimator_preproc.evaluate() << " " << estimator_network.evaluate() << std::endl;
        }
    }
    catch (std::exception &err) {
        std::cerr << err.what() << std::endl;
        return 1;
    }

    return 0;
}