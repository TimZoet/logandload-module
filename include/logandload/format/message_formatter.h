#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/format/parameter_formatter.h"

namespace lal
{
    /**
     * \brief The MessageFormatter processes a single message format string with a unique combination of dynamic parameters.
     */
    class MessageFormatter
    {
    public:
        ////////////////////////////////////////////////////////////////
        // Constructors.
        ////////////////////////////////////////////////////////////////

        MessageFormatter();

        MessageFormatter(std::string                      formatMessage,
                         uint32_t                         category,
                         const std::vector<ParameterKey>& parameters,
                         const ParameterFormatterMap&     parameterFormatters);

        MessageFormatter(const MessageFormatter&) = delete;

        MessageFormatter(MessageFormatter&&) noexcept = delete;

        ~MessageFormatter() noexcept;

        MessageFormatter& operator=(const MessageFormatter&) = delete;

        MessageFormatter& operator=(MessageFormatter&&) noexcept = delete;

        ////////////////////////////////////////////////////////////////
        // Getters.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Get format message.
         * \return Format message.
         */
        [[nodiscard]] const std::string& getMessage() const noexcept;

        /**
         * \brief Get category.
         * \return Category.
         */
        [[nodiscard]] uint32_t getCategory() const noexcept;

        ////////////////////////////////////////////////////////////////
        // Format.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Read binary message data from the istream and write a formatted string to the ostream.
         * \param in Binary istream.
         * \param out String ostream.
         */
        void format(std::istream& in, std::ostream& out) const;

    private:
        /**
         * \brief Message string.
         */
        std::string message;

        /**
         * \brief Message category.
         */
        uint32_t category = 0;

        /**
         * \brief List of substrings inbetween dynamic parameters.
         */
        std::vector<std::string_view> substrings;

        /**
         * \brief List of formatters for dynamic parameters.
         */
        std::vector<IParameterFormatter*> formatters;
    };

    using MessageFormatterPtr = std::unique_ptr<MessageFormatter>;
    using MessageFormatterMap = std::unordered_map<MessageKey, MessageFormatterPtr>;
}  // namespace lal
