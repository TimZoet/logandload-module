#include "logandload/format/message_formatter.h"

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <format>

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/utils/lal_error.h"

namespace lal
{
    ////////////////////////////////////////////////////////////////
    // Constructors.
    ////////////////////////////////////////////////////////////////

    MessageFormatter::MessageFormatter() = default;

    MessageFormatter::MessageFormatter(std::string                      formatMessage,
                                       const uint32_t                   category,
                                       const std::vector<ParameterKey>& parameters,
                                       const ParameterFormatterMap&     parameterFormatters) :
        message(std::move(formatMessage)), category(category)
    {
        const auto indices = getParameterIndices(message);
        for (size_t i = 0; i < indices.size() + 1; i++)
        {
            // Calculate start and end index of substring preceding {}.
            const auto start = static_cast<std::make_signed_t<size_t>>(i == 0 ? 0 : indices[i - 1] + 2);
            const auto end = static_cast<std::make_signed_t<size_t>>(i == indices.size() ? message.size() : indices[i]);
            if (end - start)
                substrings.emplace_back(message.begin() + start, message.begin() + end);
            else
                substrings.emplace_back(message.begin(), message.begin());

            if (i < indices.size())
            {
                auto it = parameterFormatters.find(parameters[i]);
                if (it == parameterFormatters.end())
                    throw LalError(std::format("Could not find parameter {}.", parameters[i].key));

                formatters.push_back(it->second.get());
            }
        }
    }

    MessageFormatter::~MessageFormatter() noexcept = default;

    ////////////////////////////////////////////////////////////////
    // Getters.
    ////////////////////////////////////////////////////////////////

    const std::string& MessageFormatter::getMessage() const noexcept { return message; }

    uint32_t MessageFormatter::getCategory() const noexcept { return category; }

    ////////////////////////////////////////////////////////////////
    // Format.
    ////////////////////////////////////////////////////////////////

    void MessageFormatter::format(std::istream& in, std::ostream& out) const
    {
        for (size_t i = 0; i < std::max(substrings.size(), formatters.size()); i++)
        {
            out << substrings[i];
            if (i < formatters.size()) formatters[i]->format(in, out);
        }
    }
}  // namespace lal
