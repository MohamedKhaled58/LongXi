#include "Core/Profiling/IProfileSink.h"

namespace LXCore
{

std::atomic<IProfileSink*> IProfileSink::s_Active = nullptr;

void IProfileSink::SetActive(IProfileSink* sink)
{
    s_Active = sink;
}

IProfileSink* IProfileSink::GetActive()
{
    return s_Active;
}

} // namespace LXCore
