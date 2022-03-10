#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <exception>
#include <string>
#include <source_location>

namespace lal
{
    class LalError final : public std::exception
    {
    public:
        explicit LalError(const std::string& msg, std::source_location loc = std::source_location::current());

        [[nodiscard]] const char* what() const noexcept override;

    private:
        std::string message;
    };
}  // namespace lal
