#pragma once
#include "vector_of_columns.h"
#include <complex>

namespace tgvoiprate
{

class Spectrogram
{
public:
    Spectrogram(const std::vector<float>& audioData)
        : mSpectrogram{BandsCount()}
    {
        for (int frameStart = 0; frameStart + WINDOW_SIZE < audioData.size(); frameStart += WINDOW_SIZE / 2)
        {
            ComputeSpectrogramColumn(&audioData[frameStart]);
        }
        NormalizePower();
        ConvertToDecibels();
    }

    VectorOfColumns& Data()
    {
        return mSpectrogram;
    }

    size_t BandsCount() { return mCriticalBandEdges.size() - 1; }
    size_t Length() { return mSpectrogram.Length(); }

private:
    static const int WINDOW_SIZE = 4096;
    static const int SAMPLE_RATE = 48000;

    void ComputeSpectrogramColumn(const float* frame)
    {
        static std::vector<std::complex<double>> fftFrame(WINDOW_SIZE);
        for (int i = 0; i < WINDOW_SIZE; ++i)
        {
            fftFrame[i] = frame[i] * mHammingWindow[i];
        }
        fft::transform(fftFrame);
        mSpectrogram.Append(GroupIntoCriticalBands(fftFrame));
    }

    std::vector<double> GroupIntoCriticalBands(const std::vector<std::complex<double>>& fftFrame)
    {
        std::vector<double> result(BandsCount());
        int currentBandIdx = 1;
        for (int i = 0; i < fftFrame.size(); ++i)
        {
            double frequency = (static_cast<double>(i) / WINDOW_SIZE) * SAMPLE_RATE;
            if (frequency > mCriticalBandEdges[currentBandIdx]) {
                ++currentBandIdx;
            }
            if (currentBandIdx == mCriticalBandEdges.size())
            {
                break;
            }
            if (frequency > mCriticalBandEdges[currentBandIdx - 1])
            {
                result[currentBandIdx - 1] += std::abs(fftFrame[i]);
            }
        }
        return result;
    }

    void NormalizePower()
    {
        const double epsilon = 0.001;
        double maxPower = mSpectrogram.Max();
        mSpectrogram.ForEach([maxPower, epsilon](double& x)
        {
            x = std::max(x, epsilon) / maxPower;
        });

    }

    void ConvertToDecibels()
    {
        mSpectrogram.ForEach([](double& x)
        {
            x = 20 * log10(x);
        });
    }

    static std::array<double, WINDOW_SIZE> CreateHammingWindow()
    {
        std::array<double, WINDOW_SIZE> result;
        for (int i =0; i < WINDOW_SIZE; ++i)
        {
            result[i] = 0.53836 - 0.46164 * cos((M_PI_2 * i) / WINDOW_SIZE);
        }
        return result;
    }

    VectorOfColumns mSpectrogram;
    const std::array<double, WINDOW_SIZE> mHammingWindow = CreateHammingWindow();
    const std::array<int, 16> mCriticalBandEdges
    {
        150,  250,  350,  450,  570,  700,  840, 1000,
        1170, 1370, 1600, 1850, 2150, 2500, 2900, 3400
    };
};

}
