#include "Renderer/Renderer.h"

#include "Renderer/DX11Renderer.h"

#include <memory>

namespace LongXi
{

std::unique_ptr<Renderer> CreateRenderer()
{
    return std::make_unique<DX11Renderer>();
}

} // namespace LongXi
