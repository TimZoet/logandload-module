#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <utility>

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/log/format_type.h"
#include "logandload/format/format_state.h"
#include "logandload/format/message_formatter.h"

namespace lal
{
    class Formatter
    {
    public:
        ////////////////////////////////////////////////////////////////
        // Constructors.
        ////////////////////////////////////////////////////////////////

        Formatter();

        Formatter(const Formatter&) = delete;

        Formatter(Formatter&&) = delete;

        ~Formatter() noexcept;

        Formatter& operator=(const Formatter&) = delete;

        Formatter& operator=(Formatter&&) = delete;

        ////////////////////////////////////////////////////////////////
        // ...
        ////////////////////////////////////////////////////////////////

        template<typename T, typename... Ts>
        void registerParameter(std::function<void(std::ostream&, const T&)> f,
                               std::function<void(std::ostream&, const Ts&)>... fs)
        {
            parameterFormatters.try_emplace(ParameterFormatter<T>::key,
                                            std::make_unique<ParameterFormatter<T>>(std::move(f)));

            // Recurse if there are types left.
            if constexpr (sizeof...(Ts) > 0) registerParameter<Ts...>(fs...);
        }

        bool format(const std::filesystem::path& path);

    private:
        /**
         * \brief Read a format file and construct a message formatter for each format type in the file.
         * \param fmtPath Path to format file.
         * \return Map of message formatters.
         */
        std::pair<bool, MessageFormatterMap> createFormatters(const std::filesystem::path& fmtPath);

        /**
         * \brief Read log file, format messages and write to one or more output files (one per stream in log).
         * \param path Path to log file.
         * \param messageOrder Messages are ordered and include an index.
         * \param messageFormatters Map of message formatters.
         */
        void writeLog(const std::filesystem::path& path, bool messageOrder, MessageFormatterMap& messageFormatters);

        /**
         * \brief Write an anonymous region start message to the output stream.
         * \param out Output stream.
         * \param state State.
         */
        void writeAnonymousRegionStart(std::ostream& out, FormatState& state) const;

        /**
         * \brief Write a named region start message to the output stream.
         * \param messageFormatters Map of message formatters.
         * \param in Input stream.
         * \param out Output stream.
         * \param state State.
         */
        void writeNamedRegionStart(MessageFormatterMap& messageFormatters,
                                   std::istream&        in,
                                   std::ostream&        out,
                                   FormatState&         state) const;

        /**
         * \brief Write a region end message to the output stream.
         * \param out Output stream.
         * \param state State.
         */
        void writeRegionEnd(std::ostream& out, FormatState& state) const;

        /**
         * \brief Write a source information message to the output stream.
         * \param messageFormatters Map of message formatters.
         * \param in Input stream.
         * \param out Output stream.
         */
        void writeSourceInfo(MessageFormatterMap& messageFormatters, std::istream& in, std::ostream& out);

        /**
         * \brief  Write a formatted message to the output stream.
         * \param messageFormatters Map of message formatters.
         * \param in Input stream.
         * \param out Output stream.
         * \param key Message key.
         * \param state State.
         * \param order If true, messages are ordered and message index must be read and written.
         */
        void writeMessage(MessageFormatterMap& messageFormatters,
                          std::istream&        in,
                          std::ostream&        out,
                          MessageKey           key,
                          FormatState&         state,
                          bool                 order) const;

        ////////////////////////////////////////////////////////////////
        // Member variables.
        ////////////////////////////////////////////////////////////////

        ParameterFormatterMap parameterFormatters;

    public:
        /**
         * \brief Function for generating an output filename. First parameter is path to input log file. Second parameter is stream index.
         */
        std::function<std::filesystem::path(const std::filesystem::path&, size_t)> filenameFormatter;

        /**
         * \brief Function for writing the message category.
         */
        std::function<void(std::ostream&, uint32_t)> categoryFormatter;

        /**
         * \brief Function for writing the message index.
         */
        std::function<void(std::ostream&, uint64_t)> indexFormatter;

        /**
         * \brief Number of characters by which the default indexFormatter pads the message index.
         */
        int32_t indexPaddingWidth = 8;

        /**
         * \brief Character with which the default indexFormatter pads the message index.
         */
        char indexPaddingCharacter = '0';

        /**
         * \brief Function for writing anonymous regions. Second parameter indicates if a start (true) or end (false) region must be written.
         */
        std::function<void(std::ostream&, bool)> anonymousRegionFormatter;

        /**
         * \brief Function for writing named regions. Second parameter indicates if a start (true) or end (false) region must be written.
         */
        std::function<void(std::ostream&, bool, const std::string&)> namedRegionFormatter;

        /**
         * \brief Number of characters with which the default anonymousRegionFormatter and namedRegionFormatter pad a region.
         */
        uint32_t regionIndent = 2;

        /**
         * \brief Character with which the default anonymousRegionFormatter and namedRegionFormatter pad a region.
         */
        char regionIndentCharacter = ' ';
    };
}  // namespace lal
