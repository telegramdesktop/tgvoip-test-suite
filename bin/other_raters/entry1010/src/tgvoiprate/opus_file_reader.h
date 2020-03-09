#pragma once

#include <opusfile.h>
#include <string>
#include <vector>

namespace tgvoiprate {

class OpusFileReader
{
public:
    OpusFileReader(const std::string& path);
    ~OpusFileReader();

    std::vector<float> ReadAll();
    bool IsEndReached();

private:
    size_t ReadDataLessOrEqual(float* buffer, size_t size);

    OggOpusFile* mOpusFile = nullptr;
};

}
