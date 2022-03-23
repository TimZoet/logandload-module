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

        /**
         * \brief Returns whether this object has the same message string and category as the given format type.
         * \tparam F Format type.
         * \return True or false.
         */
        template<typename F>
        requires(has_message<F>&& has_category<F>) [[nodiscard]] bool matches() const noexcept
        {
            return message == F::template message && category == F::template category;
        }

        /**
         * \brief Returns whether the parameters of this object match the given list of parameters.
         * \param params List of parameter keys. A value of 0 matches any parameter type.
         * \return True or false.
         */
        [[nodiscard]] bool matches(const std::vector<ParameterKey>& params) const noexcept;

        ////////////////////////////////////////////////////////////////
        // Member variables.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Unique message key.
         */
        MessageKey key;

        /**
         * \brief Hash of the message string.
         */
        MessageKey messageHash;

        /**
         * \brief Message string.
         */
        std::string message;

        /**
         * \brief Message category.
         */
        uint32_t category = 0;

        /**
         * \brief Parameter keys.
         */
        std::vector<ParameterKey> parameters;

        /**
         * \brief Size of each parameter in bytes.
         */
        std::vector<size_t> parameterSize;

        /**
         * \brief Sum of sizeof of all parameters.
         */
        size_t messageSize = 0;
    };
}  // namespace lal
