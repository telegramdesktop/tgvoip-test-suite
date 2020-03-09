#pragma once

#include <cmath>


namespace tgvoipcontest::dsp {

unsigned long nextpow2(unsigned long X);

void iir_filter(
    const float* h, unsigned long Nsos,
    float* x, unsigned long Nx
);

static constexpr float TWO_PI = M_PI * 2;


class FFTContext {
private:
    unsigned long swap_initialised = 0;
    unsigned long log_2n{};
    unsigned long* butter{};
    unsigned long* bit_swap{};
    float* phi{};

public:
    void real_fwd(float* x, size_t N);

    void real_inv(float* x, size_t N);

    unsigned long correlations(
        float* x1, unsigned long n1,
        const float* x2, unsigned long n2,
        float* y
    );

    ~FFTContext() {
        free_ctx();
    }

private:
    void free_ctx();

    void init_ctx(size_t N);

    void complex_fwd(float* x, size_t N);

    void complex_inv(float* x, size_t N);
};

}
