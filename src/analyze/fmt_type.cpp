#include "logandload/analyze/fmt_type.h"

namespace lal
{
    ////////////////////////////////////////////////////////////////
    // Constructors.
    ////////////////////////////////////////////////////////////////

    FormatType::FormatType() = default;

    FormatType::~FormatType() noexcept = default;

    ////////////////////////////////////////////////////////////////
    // ...
    ////////////////////////////////////////////////////////////////

    bool FormatType::matches(const std::vector<ParameterKey>& params) const noexcept
    {
        if (params.size() != parameters.size()) return false;

        // Look for any mismatch.
        for (size_t i = 0; i < params.size(); i++)
        {
            // A parameter key of 0 indicates we want to match any parameter type.
            if (params[i].key != 0 && params[i] != parameters[i]) return false;
        }

        return true;
    }
}  // namespace lal
