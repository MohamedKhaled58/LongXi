#include <memory>

#include "Renderer/DX11Renderer.h"
#include "Renderer/Renderer.h"

namespace LXEngine
{

std::unique_ptr<Renderer> CreateRenderer()
{
    return std::make_unique<DX11Renderer>();
}

} // namespace LXEngine
