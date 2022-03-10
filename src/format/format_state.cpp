#include "logandload/format/format_state.h"

namespace lal
{
    FormatState::FormatState(const uint32_t indent, const char character) : indent(indent), character(character) {}

    void FormatState::pushRegion(std::string name)
    {
        regionStack.emplace_back(std::move(name));
        regionPrepend.resize(regionPrepend.size() + indent, character);
    }

    std::string FormatState::popRegion()
    {
        auto name = regionStack.back();
        regionStack.pop_back();
        regionPrepend.resize(regionPrepend.size() - indent);
        return name;
    }

    std::string& FormatState::getRegionPrepend() { return regionPrepend; }
}  // namespace lal
