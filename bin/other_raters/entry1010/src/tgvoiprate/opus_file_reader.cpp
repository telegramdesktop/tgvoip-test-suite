#include "opus_file_reader.h"
#include "utils.h"
#include <cstring>

namespace tgvoiprate {

OpusFileReader::OpusFileReader(const std::string& path)
{
    int error = 0;
    mOpusFile = op_open_file(path.c_str(), &error);
    throwIf(error != 0, "unable to open opus file '" + path + "', error code: " + std::to_string(error));
}

OpusFileReader::~OpusFileReader()
{
    if (mOpusFile != nullptr)
    {
        op_free(mOpusFile);
    }
}

std::vector<float> OpusFileReader::ReadAll()
{
    std::vector<float> result(65536);
    size_t readSize = 0;
    while (!IsEndReached())
    {
        readSize += ReadDataLessOrEqual(result.data() + readSize, result.size() - readSize);
        if (readSize == result.size())
        {
            result.resize(readSize * 2);
        }
    }
    result.resize(readSize);
    return result;
}

bool OpusFileReader::IsEndReached()
{
    return op_raw_total(mOpusFile, -1) == op_raw_tell(mOpusFile);
}

size_t OpusFileReader::ReadDataLessOrEqual(float* buffer, size_t size)
{
    int link;
    int samplesCount = op_read_float(mOpusFile, buffer, size, &link);
    throwIf(samplesCount < 0, "error while reading opus file, error code: " + std::to_string(samplesCount));
    int channelCount = op_channel_count(mOpusFile, link);
    throwIf(channelCount != 1, "error while reading opus file: only monophonic files are supported");
    return static_cast<size_t>(samplesCount);
}

}
