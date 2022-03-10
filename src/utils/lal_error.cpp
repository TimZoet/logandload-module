#include "logandload/utils/lal_error.h"

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <format>

namespace lal
{
    LalError::LalError(const std::string& msg, const std::source_location loc)
    {
        message = std::format("\"{}\" in {} at {}:{}", msg, loc.file_name(), loc.line(), loc.column());
    }

    const char* LalError::what() const noexcept { return message.c_str(); }
}  // namespace lal
