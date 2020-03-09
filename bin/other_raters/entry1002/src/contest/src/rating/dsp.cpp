#include <cmath>
#include <numeric>
#include <algorithm>

#include "dsp.hpp"


namespace tgvoipcontest::dsp {

unsigned long nextpow2(size_t X) {
    unsigned long C = 1;
    while ((C < std::numeric_limits<decltype(C)>::max()) && (C < X))
        C <<= 1u;

    return C;
}

static void iir_sos_filter(
    float* x, unsigned long Nx,
    float b0, float b1, float b2, float a1, float a2
) {
    float z0;
    float z1 = 0.0f;
    float z2 = 0.0f;

    if ((a1 != 0.0f) || (a2 != 0.0f)) {
        if ((b1 != 0.0f) || (b2 != 0.0f)) {
            while ((Nx) > 0) {
                Nx--;
                z0 = (*x) - a1 * z1 - a2 * z2;
                *(x++) = b0 * z0 + b1 * z1 + b2 * z2;
                z2 = z1;
                z1 = z0;
            }
        } else {
            if (b0 != 1.0f) {
                while ((Nx) > 0) {
                    Nx--;
                    z0 = (*x) - a1 * z1 - a2 * z2;
                    *(x++) = b0 * z0;
                    z2 = z1;
                    z1 = z0;
                }
            } else {
                while ((Nx) > 0) {
                    Nx--;
                    z0 = (*x) - a1 * z1 - a2 * z2;
                    *(x++) = z0;
                    z2 = z1;
                    z1 = z0;
                }
            }
        }
    } else {
        if ((b1 != 0.0f) || (b2 != 0.0f)) {
            while ((Nx) > 0) {
                Nx--;
                z0 = (*x);
                *(x++) = b0 * z0 + b1 * z1 + b2 * z2;
                z2 = z1;
                z1 = z0;
            }
        } else {
            if (b0 != 1.0f) {
                while ((Nx) > 0) {
                    Nx--;
                    *x = b0 * (*x);
                    x++;
                }
            }
        }
    }
}


void iir_filter(
    const float* h, unsigned long Nsos,
    float* x, unsigned long Nx
) {
    for (size_t C = 0; C < Nsos; C++) {
        iir_sos_filter(x, Nx, h[0], h[1], h[2], h[3], h[4]);
        h += 5;
    }
}

void FFTContext::real_fwd(float* x, size_t N) {
    auto* y = new float[2 * N];

    for (size_t i = 0; i < N; i++) {
        y[2 * i] = x[i];
        y[2 * i + 1] = 0.0f;
    }

    complex_fwd(y, N);

    for (size_t i = 0; i <= N / 2; i++) {
        x[2 * i] = y[2 * i];
        x[2 * i + 1] = y[2 * i + 1];
    }

    delete[] y;
}

void FFTContext::real_inv(float* x, size_t N) {
    auto* y = new float[2 * N];

    std::copy_n(x, N, y);
    for (size_t i = N / 2 + 1; i < N; i++) {
        int j = N - i;
        y[2 * i] = x[2 * j];
        y[2 * i + 1] = -x[2 * j + 1];
    }

    complex_inv(y, N);

    for (size_t i = 0; i < N; i++) {
        x[i] = y[2 * i];
    }

    delete[] y;
}

unsigned long FFTContext::correlations(float* x1, unsigned long n1, const float* x2, unsigned long n2, float* y) {
    size_t Nx = nextpow2(std::max(n1, n2));
    auto* tmp1 = new float[(2 * Nx + 2)];
    auto* tmp2 = new float[(2 * Nx + 2)];

    std::copy_n(x1, n1, tmp1);
    std::reverse(tmp1, tmp1 + n1);
    std::fill(tmp1 + n1, tmp1 + 2 * Nx, 0.0f);

    real_fwd(tmp1, 2 * Nx);

    std::copy_n(x2, n2, tmp2);
    std::fill(tmp2 + n2, tmp2 + 2 * Nx, 0.0f);

    real_fwd(tmp2, 2 * Nx);

    for (size_t C = 0; C <= Nx; C++) {
        size_t D = C << 1u;
        float r1 = tmp1[D];
        float i1 = tmp1[D + 1];
        tmp1[D] = r1 * tmp2[D] - i1 * tmp2[1 + D];
        tmp1[1 + D] = r1 * tmp2[1 + D] + i1 * tmp2[D];
    }

    real_inv(tmp1, 2 * Nx);
    size_t Ny = n1 + n2 - 1;
    std::copy_n(tmp1, Ny, y);

    delete[] tmp1;
    delete[] tmp2;

    return Ny;
}

void FFTContext::free_ctx() {
    if (swap_initialised != 0) {
        delete[] butter;
        delete[] bit_swap;
        delete[] phi;
        swap_initialised = 0;
    }
}

void FFTContext::init_ctx(size_t N) {
    if ((swap_initialised != N) && (swap_initialised != 0))
        free_ctx();

    if (swap_initialised == N) {
        return;
    } else {
        size_t C = N;
        for (log_2n = 0; C > 1; C >>= 1u)
            log_2n++;

        C = 1;
        C <<= log_2n;
        if (N == C)
            swap_initialised = N;

        butter = new unsigned long[N >> 1u];
        bit_swap = new unsigned long[N];
        phi = new float[2 * (N >> 1u)];

        for (size_t i = 0, j = 0; i < (N >> 1u); i++) {
            float Theta = (TWO_PI * i) / N;
            phi[j++] = std::cos(Theta);
            phi[j++] = std::sin(Theta);
        }

        butter[0] = 0;
        size_t L = 1;
        size_t K = N >> 2u;
        while (K >= 1) {
            for (size_t i = 0; i < L; i++)
                butter[i + L] = butter[i] + K;
            L <<= 1u;
            K >>= 1u;
        }
    }
}

void FFTContext::complex_fwd(float* x, size_t N) {
    unsigned long Cycle, C, S, NC;
    unsigned long Step = N >> 1u;
    unsigned long K1, K2;
    float R1, I1, R2, I2;
    float ReFFTPhi, ImFFTPhi;

    if (N > 1) {
        init_ctx(N);

        for (Cycle = 1; Cycle < N; Cycle <<= 1u, Step >>= 1u) {
            K1 = 0;
            K2 = Step << 1u;

            for (C = 0; C < Cycle; C++) {
                NC = butter[C] << 1u;
                ReFFTPhi = phi[NC];
                ImFFTPhi = phi[NC + 1];
                for (S = 0; S < Step; S++) {
                    R1 = x[K1];
                    I1 = x[K1 + 1];
                    R2 = x[K2];
                    I2 = x[K2 + 1];

                    x[K1++] = R1 + ReFFTPhi * R2 + ImFFTPhi * I2;
                    x[K1++] = I1 - ImFFTPhi * R2 + ReFFTPhi * I2;
                    x[K2++] = R1 - ReFFTPhi * R2 - ImFFTPhi * I2;
                    x[K2++] = I1 + ImFFTPhi * R2 - ReFFTPhi * I2;
                }
                K1 = K2;
                K2 = K1 + (Step << 1u);
            }
        }

        NC = N >> 1u;
        for (C = 0; C < NC; C++) {
            bit_swap[C] = butter[C] << 1u;
            bit_swap[C + NC] = 1 + bit_swap[C];
        }
        for (C = 0; C < N; C++)
            if ((S = bit_swap[C]) != C) {
                bit_swap[S] = S;
                K1 = C << 1u;
                K2 = S << 1u;
                R1 = x[K1];
                x[K1++] = x[K2];
                x[K2++] = R1;
                R1 = x[K1];
                x[K1] = x[K2];
                x[K2] = R1;
            }
    }
}

void FFTContext::complex_inv(float* x, size_t N) {
    unsigned long Cycle, C, S, NC;
    unsigned long Step = N >> 1u;
    unsigned long K1, K2;
    float R1, I1, R2, I2;
    float ReFFTPhi, ImFFTPhi;

    if (N > 1) {
        init_ctx(N);

        for (Cycle = 1; Cycle < N; Cycle <<= 1u, Step >>= 1u) {
            K1 = 0;
            K2 = Step << 1u;

            for (C = 0; C < Cycle; C++) {
                NC = butter[C] << 1u;
                ReFFTPhi = phi[NC];
                ImFFTPhi = phi[NC + 1];
                for (S = 0; S < Step; S++) {
                    R1 = x[K1];
                    I1 = x[K1 + 1];
                    R2 = x[K2];
                    I2 = x[K2 + 1];

                    x[K1++] = R1 + ReFFTPhi * R2 - ImFFTPhi * I2;
                    x[K1++] = I1 + ImFFTPhi * R2 + ReFFTPhi * I2;
                    x[K2++] = R1 - ReFFTPhi * R2 + ImFFTPhi * I2;
                    x[K2++] = I1 - ImFFTPhi * R2 - ReFFTPhi * I2;
                }
                K1 = K2;
                K2 = K1 + (Step << 1u);
            }
        }

        NC = N >> 1u;
        for (C = 0; C < NC; C++) {
            bit_swap[C] = butter[C] << 1u;
            bit_swap[C + NC] = 1 + bit_swap[C];
        }
        for (C = 0; C < N; C++)
            if ((S = bit_swap[C]) != C) {
                bit_swap[S] = S;
                K1 = C << 1u;
                K2 = S << 1u;
                R1 = x[K1];
                x[K1++] = x[K2];
                x[K2++] = R1;
                R1 = x[K1];
                x[K1] = x[K2];
                x[K2] = R1;
            }

        NC = N << 1u;
        for (C = 0; C < NC;)
            x[C++] /= N;
    }
}
}