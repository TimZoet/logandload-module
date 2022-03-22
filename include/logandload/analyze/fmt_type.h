#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <string>
#include <vector>

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/log/format_type.h"

namespace lal
{
    class FormatType
    {
    public:
        ////////////////////////////////////////////////////////////////
        // Constructors.
        ////////////////////////////////////////////////////////////////

        FormatType();

        FormatType(const FormatType&) = delete;

        FormatType(FormatType&&) = default;

        ~FormatType() noexcept;

        FormatType& operator=(const FormatType&) = delete;

        FormatType& operator=(FormatType&&) = default;

        ////////////////////////////////////////////////////////////////
        // ...
        ////////////////////////////////////////////////////////////////

        template<typename F>
        requires(has_message<F>&& has_category<F>) [[nodiscard]] bool matches() const noexcept
        {
            return message == F::template message && category == F::template category;
        }

        ////////////////////////////////////////////////////////////////
        // Member variables.
        ////////////////////////////////////////////////////////////////

        MessageKey key;

        MessageKey messageHash;

        std::string message;

        uint32_t category = 0;

        std::vector<ParameterKey> parameters;

        /**
         * \brief Sum of sizeof of all parameters.
         */
        size_t messageSize = 0;
    };
}  // namespace lal
