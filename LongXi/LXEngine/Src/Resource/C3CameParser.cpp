#include "Resource/C3CameParser.h"

#include <string>

#include "Resource/C3BinaryReader.h"
#include "Resource/C3Log.h"

namespace LongXi
{

namespace
{
bool ReadLengthPrefixedString(C3BinaryReader& reader, std::string& out)
{
    uint32_t length = 0;
    if (!reader.Read(length))
    {
        return false;
    }

    if (length == 0)
    {
        out.clear();
        return true;
    }

    std::vector<uint8_t> bytes;
    if (!reader.ReadBytes(length, bytes))
    {
        return false;
    }

    out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

} // namespace

bool C3CameParser::Parse(const std::vector<uint8_t>& data, CameraPathResource& outCamera)
{
    outCamera         = CameraPathResource{};
    outCamera.rawData = data;

    C3BinaryReader reader(data);

    if (!ReadLengthPrefixedString(reader, outCamera.name))
    {
        LX_C3_WARN("CAME: Failed to read camera name");
        return false;
    }

    if (!reader.Read(outCamera.fov))
    {
        LX_C3_WARN("CAME: Failed to read FOV");
        return false;
    }

    uint32_t frameCount = 0;
    if (!reader.Read(frameCount))
    {
        LX_C3_WARN("CAME: Failed to read frame count");
        return false;
    }

    outCamera.keyframes.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i)
    {
        Vector3 from{};
        if (!reader.Read(from))
        {
            LX_C3_WARN("CAME: Failed to read from position (frame={})", i);
            return false;
        }
        outCamera.keyframes[i].time     = static_cast<float>(i);
        outCamera.keyframes[i].position = from;
        outCamera.keyframes[i].fov      = outCamera.fov;
    }

    for (uint32_t i = 0; i < frameCount; ++i)
    {
        Vector3 to{};
        if (!reader.Read(to))
        {
            LX_C3_WARN("CAME: Failed to read target position (frame={})", i);
            return false;
        }
        outCamera.keyframes[i].target = to;
    }

    return true;
}

} // namespace LongXi
