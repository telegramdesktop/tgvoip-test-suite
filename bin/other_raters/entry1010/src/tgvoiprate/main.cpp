#include "cmd_args.h"
#include "opus_file_reader.h"
#include "fft.h"
#include "vector_of_columns.h"
#include "nsim.h"
#include "spectorgram.h"
#include <algorithm>
#include <fstream>
#include <functional>
#include <cassert>
#include <iostream>
#include <cmath>
#include <map>

using namespace tgvoiprate;

std::vector<std::pair<int, double>> CalcOffsetsAndSimilaritiesForFrames(VectorOfColumns& original, VectorOfColumns& degraded)
{
    int frameLength = 10;
    int step = frameLength;
    double cutoffMean = original.Mean();
    std::vector<std::pair<int, double>> result;
    original.ForEachFrame(frameLength, step, [&](int idxOrig, VectorOfColumns& frameOrig)
        {
            int maxSimIdx = -1;
            double maxSimValue = 0;
            int searchLength = std::min(100 + frameLength, static_cast<int>(degraded.Length()) - idxOrig);
            if (searchLength < 0) { return; }
            if (frameOrig.Mean() < cutoffMean) { return; }
            degraded.SubCopy(idxOrig, searchLength).ForEachFrame(frameLength, 1,
            [&](int idxDegraded, VectorOfColumns& frameDegraded)
            {
                if (frameDegraded.Mean() < cutoffMean) { return; }
                double nsim = NSIM(frameOrig, frameDegraded);
                if (nsim > maxSimValue && nsim < 3)
                {
                    maxSimValue = nsim;
                    maxSimIdx = idxDegraded;
                }
            });
            result.emplace_back(maxSimIdx, maxSimValue);
        });
        return result;
}

double calcOffsetStddev(const std::vector<std::pair<int, double>>& offsetsAndSimilarities)
{
    VectorOfColumns offsets(1);

    std::map<int, int> offsetsCount;
    for (auto& elem: offsetsAndSimilarities)
    {
        if (elem.first > 0) { offsets.Append({static_cast<double>(elem.first)}); }
    }
    double meanOffset = offsets.Mean();
    double x = offsets.Accumulate(0, [meanOffset](double result, double value){ return result + pow(value - meanOffset, 2); });
    return sqrt(x / offsets.Length());
}

double calcMeanSimilarity(const std::vector<std::pair<int, double>>& offsetsAndSimilarities)
{
    VectorOfColumns similarities(1);
    for (auto& elem: offsetsAndSimilarities)
    {
        if (elem.first > 0) { similarities.Append({elem.second}); }
    }
    return similarities.Mean();
}

double calcUnrecognizedPercentage(const std::vector<std::pair<int, double>>& offsetsAndSimilarities)
{
    double unrecognizedCount;
    for (auto& elem: offsetsAndSimilarities)
    {
        if (elem.first < 0) { unrecognizedCount += 1; }
    }
    return unrecognizedCount / offsetsAndSimilarities.size();
}

double scoreToGrade(double score, double startGrade, std::vector<std::pair<double, double>> gradeBins)
{
    double result = startGrade;
    for (auto& bin: gradeBins)
    {
        if (score > bin.first) { result = bin.second; }
    }
    return result;
}

int main(int argc, char** argv)
{
    try
    {
        tgvoiprate::CmdArgs args{argc, argv};
        Spectrogram original{tgvoiprate::OpusFileReader{args.initialSoundPath}.ReadAll()};
        Spectrogram degraded{tgvoiprate::OpusFileReader{args.corruptedSoundPath}.ReadAll()};

        auto offestsAndSimilarities = CalcOffsetsAndSimilaritiesForFrames(original.Data(), degraded.Data());

        double offsetStddev = calcOffsetStddev(offestsAndSimilarities);
        double meanSimilarity = calcMeanSimilarity(offestsAndSimilarities);
        double unrecoginizedPerc = calcUnrecognizedPercentage(offestsAndSimilarities);

        double grade1 = scoreToGrade(unrecoginizedPerc, 1.0, {{0.1, 0.5}, {0.27, 0.1}});
        double grade2 = scoreToGrade(meanSimilarity, 0.5, {{1.969195, 0.65}, {1.980665, 0.8}, {1.983225, 0.9}, {1.986578, 1}});
        double grade3 = scoreToGrade(offsetStddev, 1.0, {{9.0, 0.95}, {16, 0.8}, {20, 0.7}, {26, 0.5}});

        std::cout << grade1 * grade2 * grade3 * 4 + 1 << std::endl;
    }
    catch (const std::runtime_error& err)
    {
        std::cout << "ERROR: " << err.what() << std::endl;
        return 1;
    }
    return 0;
}
