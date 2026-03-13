#include "Resource/C3PtclParser.h"

#include <cstring>

#include "Resource/C3BinaryReader.h"
#include "Resource/C3Log.h"

namespace LongXi
{

namespace
{
Matrix4 IdentityMatrix()
{
    Matrix4 m{};
    m.m[0]  = 1.0f;
    m.m[5]  = 1.0f;
    m.m[10] = 1.0f;
    m.m[15] = 1.0f;
    return m;
}

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

bool C3PtclParser::Parse(const std::vector<uint8_t>& data, ParticleResource& outParticle, bool isPtcl3)
{
    outParticle         = ParticleResource{};
    outParticle.rawData = data;
    outParticle.isPtcl3 = isPtcl3;

    C3BinaryReader reader(data);

    if (!ReadLengthPrefixedString(reader, outParticle.name))
    {
        LX_C3_WARN("PTCL: Failed to read name");
        return false;
    }

    if (!ReadLengthPrefixedString(reader, outParticle.textureName))
    {
        LX_C3_WARN("PTCL: Failed to read texture name");
        return false;
    }

    if (!reader.Read(outParticle.rowCount))
    {
        LX_C3_WARN("PTCL: Failed to read row count");
        return false;
    }

    if (!isPtcl3)
    {
        if (!reader.Read(outParticle.maxParticles))
        {
            LX_C3_WARN("PTCL: Failed to read max particles");
            return false;
        }

        if (!reader.Read(outParticle.frameCount))
        {
            LX_C3_WARN("PTCL: Failed to read frame count");
            return false;
        }

        outParticle.frames.resize(outParticle.frameCount);
        for (uint32_t f = 0; f < outParticle.frameCount; ++f)
        {
            ParticleFrame& frame = outParticle.frames[f];
            if (!reader.Read(frame.count))
            {
                LX_C3_WARN("PTCL: Failed to read frame count (frame={})", f);
                return false;
            }

            if (frame.count > 0)
            {
                frame.positions.resize(frame.count);
                frame.ages.resize(frame.count);
                frame.sizes.resize(frame.count);

                if (!reader.ReadBytes(frame.count * sizeof(Vector3), reinterpret_cast<uint8_t*>(frame.positions.data())))
                {
                    LX_C3_WARN("PTCL: Failed to read positions (frame={})", f);
                    return false;
                }

                if (!reader.ReadBytes(frame.count * sizeof(float), reinterpret_cast<uint8_t*>(frame.ages.data())))
                {
                    LX_C3_WARN("PTCL: Failed to read ages (frame={})", f);
                    return false;
                }

                if (!reader.ReadBytes(frame.count * sizeof(float), reinterpret_cast<uint8_t*>(frame.sizes.data())))
                {
                    LX_C3_WARN("PTCL: Failed to read sizes (frame={})", f);
                    return false;
                }

                if (!reader.Read(frame.matrix))
                {
                    LX_C3_WARN("PTCL: Failed to read transform matrix (frame={})", f);
                    return false;
                }
            }
            else
            {
                frame.matrix = IdentityMatrix();
            }
        }

        return true;
    }

    if (!reader.Read(outParticle.behavior1) || !reader.Read(outParticle.behavior2))
    {
        LX_C3_WARN("PTC3: Failed to read behavior bytes");
        return false;
    }

    if (!reader.Skip(8))
    {
        LX_C3_WARN("PTC3: Failed to skip reserved bytes");
        return false;
    }

    if (!reader.Read(outParticle.scaleX) || !reader.Read(outParticle.scaleY) || !reader.Read(outParticle.scaleZ))
    {
        LX_C3_WARN("PTC3: Failed to read scale parameters");
        return false;
    }

    if (!reader.Skip(8))
    {
        LX_C3_WARN("PTC3: Failed to skip reserved bytes (2)");
        return false;
    }

    if (!reader.Read(outParticle.rotationSpeed))
    {
        LX_C3_WARN("PTC3: Failed to read rotation speed");
        return false;
    }

    if (!reader.Read(outParticle.maxAlpha) || !reader.Read(outParticle.minAlpha))
    {
        LX_C3_WARN("PTC3: Failed to read alpha range");
        return false;
    }

    uint32_t lifetimeBits = 0;
    if (!reader.Read(lifetimeBits))
    {
        LX_C3_WARN("PTC3: Failed to read lifetime bits");
        return false;
    }
    std::memcpy(&outParticle.totalLifetime, &lifetimeBits, sizeof(float));

    if (!reader.Read(outParticle.fadeStartAge))
    {
        LX_C3_WARN("PTC3: Failed to read fade start age");
        return false;
    }

    if (!reader.Read(outParticle.maxParticles))
    {
        LX_C3_WARN("PTC3: Failed to read max particles");
        return false;
    }

    if (!reader.Read(outParticle.frameCount))
    {
        LX_C3_WARN("PTC3: Failed to read frame count");
        return false;
    }

    outParticle.frames.resize(outParticle.frameCount);
    for (uint32_t f = 0; f < outParticle.frameCount; ++f)
    {
        ParticleFrame& frame = outParticle.frames[f];
        if (!reader.Read(frame.count))
        {
            LX_C3_WARN("PTC3: Failed to read frame count (frame={})", f);
            return false;
        }

        if (frame.count > 0)
        {
            frame.indices.resize(frame.count);
            if (!reader.ReadBytes(frame.count * sizeof(uint16_t), reinterpret_cast<uint8_t*>(frame.indices.data())))
            {
                LX_C3_WARN("PTC3: Failed to read indices (frame={})", f);
                return false;
            }

            frame.positions.resize(frame.count);
            frame.ages.resize(frame.count);
            frame.sizes.resize(frame.count);

            if (!reader.ReadBytes(frame.count * sizeof(Vector3), reinterpret_cast<uint8_t*>(frame.positions.data())))
            {
                LX_C3_WARN("PTC3: Failed to read positions (frame={})", f);
                return false;
            }

            if (!reader.ReadBytes(frame.count * sizeof(float), reinterpret_cast<uint8_t*>(frame.ages.data())))
            {
                LX_C3_WARN("PTC3: Failed to read ages (frame={})", f);
                return false;
            }

            if (!reader.ReadBytes(frame.count * sizeof(float), reinterpret_cast<uint8_t*>(frame.sizes.data())))
            {
                LX_C3_WARN("PTC3: Failed to read sizes (frame={})", f);
                return false;
            }

            if (!reader.Read(frame.matrix))
            {
                LX_C3_WARN("PTC3: Failed to read transform matrix (frame={})", f);
                return false;
            }
        }
        else
        {
            frame.matrix = IdentityMatrix();
        }
    }

    return true;
}

} // namespace LongXi
