#include "logandload/format/formatter.h"

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <format>
#include <ranges>

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/utils/lal_error.h"

namespace lal
{
    ////////////////////////////////////////////////////////////////
    // Constructors.
    ////////////////////////////////////////////////////////////////

    Formatter::Formatter()
    {
        // Register default parameters.
        registerParameter<int8_t>([](std::ostream& out, const int8_t val) { out << val; });
        registerParameter<uint8_t>([](std::ostream& out, const uint8_t val) { out << val; });
        registerParameter<int16_t>([](std::ostream& out, const int16_t val) { out << val; });
        registerParameter<uint16_t>([](std::ostream& out, const uint16_t val) { out << val; });
        registerParameter<int32_t>([](std::ostream& out, const int32_t val) { out << val; });
        registerParameter<uint32_t>([](std::ostream& out, const uint32_t val) { out << val; });
        registerParameter<int64_t>([](std::ostream& out, const int64_t val) { out << val; });
        registerParameter<uint64_t>([](std::ostream& out, const uint64_t val) { out << val; });
        registerParameter<std::byte>([](std::ostream& out, const std::byte val) { out << static_cast<uint32_t>(val); });
        registerParameter<float>([](std::ostream& out, const float val) { out << val; });
        registerParameter<double>([](std::ostream& out, const double val) { out << val; });
        registerParameter<long double>([](std::ostream& out, const long double val) { out << val; });

        // Default filename formatting adds "_index" and replaces the last extension by .txt.
        filenameFormatter = [](const std::filesystem::path& path, const size_t index) -> std::filesystem::path {
            return path.stem().string() + "_" + std::to_string(index) + ".txt";
        };

        // Default category formatting just writes the integer with a | as separator at the end.
        categoryFormatter = [](std::ostream& out, const uint32_t c) { out << c << " | "; };

        // Default index formatting pads the index with a | as separator at the end.
        indexFormatter = [this](std::ostream& out, const uint64_t i) {
            // Get old padding settings.
            const auto width = out.width();
            const auto fill  = out.fill();
            // Set padding.
            if (indexPaddingWidth > 0) out << std::setw(indexPaddingWidth) << std::setfill(indexPaddingCharacter);
            // Write index and restore padding.
            out << i << std::setw(width) << std::setfill(fill) << " | ";
        };

        // Default anonymous region formatting.
        anonymousRegionFormatter = [](std::ostream& out, const bool start) {
            if (start)
                out << "-- REGION START: ANONYMOUS --";
            else
                out << "-- REGION END: ANONYMOUS --";
        };

        // Default named region formatting.
        namedRegionFormatter = [](std::ostream& out, const bool start, const std::string& name) {
            if (start)
                out << "-- REGION START: ";
            else
                out << "-- REGION END: ";
            out << name << " --";
        };
    }

    Formatter::~Formatter() noexcept = default;

    ////////////////////////////////////////////////////////////////
    // ...
    ////////////////////////////////////////////////////////////////

    bool Formatter::format(const std::filesystem::path& path)
    {
        auto fmtPath = path;
        fmtPath += ".fmt";
        auto [order, formatters] = createFormatters(fmtPath);
        writeLog(path, order, formatters);

        return true;
    }

    std::pair<bool, MessageFormatterMap> Formatter::createFormatters(const std::filesystem::path& fmtPath)
    {
        MessageFormatterMap formatters;

        // Open formats file.
        auto file = std::ifstream(fmtPath, std::ios::binary | std::ios::ate);
        if (!file) throw LalError(std::format("Failed to open format file {}.", fmtPath.string()));

        const auto length = file.tellg();
        file.seekg(0);

        // TODO: Skipping streamCount for now, but the writeLog could be improved by preallocating a vector of outputs instead of using a dict.
        file.seekg(sizeof(size_t));

        // Read message order setting.
        bool messageOrder = false;
        {
            int8_t _order = 0;
            file.read(reinterpret_cast<char*>(&_order), sizeof(int8_t));
            if (_order) messageOrder = true;
        }

        while (file.tellg() != length)
        {
            // Read message key.
            MessageKey key;
            file.read(reinterpret_cast<char*>(&key), sizeof(MessageKey));

            // Read format string.
            size_t len;
            file.read(reinterpret_cast<char*>(&len), sizeof(size_t));
            auto* str = new char[len];
            file.read(str, static_cast<std::make_signed_t<size_t>>(len));
            auto format = std::string(str);
            delete[] str;

            // Read category.
            uint32_t category;
            file.read(reinterpret_cast<char*>(&category), sizeof category);

            // Read all parameter keys.
            std::vector<ParameterKey> parameters;
            for (size_t i = 0; i < countParameters(format); i++)
            {
                ParameterKey paramKey;
                file.read(reinterpret_cast<char*>(&paramKey), sizeof(ParameterKey));
                parameters.push_back(paramKey);
            }

            // Add to dictionary.
            const auto [it, added] = formatters.try_emplace(
              key, std::make_unique<MessageFormatter>(std::move(format), category, parameters, parameterFormatters));

            if (!added) throw LalError("Duplicate format type key detected.");
        }

        return {messageOrder, std::move(formatters)};
    }

    void Formatter::writeLog(const std::filesystem::path& path,
                             const bool                   messageOrder,
                             MessageFormatterMap&         messageFormatters)
    {
        // Open binary log file.
        auto in = std::ifstream(path, std::ios::binary | std::ios::ate);
        if (!in) throw LalError(std::format("Failed to open log file {}.", path.string()));

        const auto length = in.tellg();
        in.seekg(0);

        std::unordered_map<size_t, std::pair<std::ofstream, FormatState>> outputs;

        while (in.tellg() != length)
        {
            // Read stream index and block size.
            size_t streamIndex = 0, blockSize = 0;
            in.read(reinterpret_cast<char*>(&streamIndex), sizeof streamIndex);
            in.read(reinterpret_cast<char*>(&blockSize), sizeof blockSize);

            // Output file and format state do not exist yet.
            if (auto it = outputs.find(streamIndex); it == outputs.end())
            {
                const auto out_path  = filenameFormatter(path, streamIndex);
                outputs[streamIndex] = std::make_pair<std::ofstream, FormatState>(
                  std::ofstream(out_path), FormatState(regionIndent, regionIndentCharacter));
            }

            // Get output file and format state.
            std::ofstream& out   = outputs[streamIndex].first;
            FormatState&   state = outputs[streamIndex].second;

            // Read block.
            auto end = in.tellg();
            end += static_cast<std::make_signed_t<size_t>>(blockSize);
            while (in.tellg() != end)
            {
                // Read message key.
                MessageKey message;
                in.read(reinterpret_cast<char*>(&message), sizeof(MessageKey));

                // Format message.
                switch (message.key)
                {
                case MessageTypes::AnonymousRegionStart.key: writeAnonymousRegionStart(out, state); break;
                case MessageTypes::NamedRegionStart.key:
                    writeNamedRegionStart(messageFormatters, in, out, state);
                    break;
                case MessageTypes::RegionEnd.key: writeRegionEnd(out, state); break;
                default: writeMessage(messageFormatters, in, out, message, state, messageOrder); break;
                }
            }
        }
    }

    void Formatter::writeAnonymousRegionStart(std::ostream& out, FormatState& state) const
    {
        out << state.getRegionPrepend();
        anonymousRegionFormatter(out, true);
        out << "\n";
        state.pushRegion("");
    }

    void Formatter::writeNamedRegionStart(MessageFormatterMap& messageFormatters,
                                          std::istream&        in,
                                          std::ostream&        out,
                                          FormatState&         state) const
    {
        MessageKey key;
        in.read(reinterpret_cast<char*>(&key), sizeof(MessageKey));
        const auto it = messageFormatters.find(key);
        if (it == messageFormatters.end()) throw LalError(std::format("Could not find named region {}.", key.key));

        const auto& format = it->second;
        out << state.getRegionPrepend();
        namedRegionFormatter(out, true, format->getMessage());
        out << "\n";
        state.pushRegion(format->getMessage());
    }

    void Formatter::writeRegionEnd(std::ostream& out, FormatState& state) const
    {
        const auto name = state.popRegion();
        out << state.getRegionPrepend();
        if (name.empty())
            anonymousRegionFormatter(out, false);
        else
            namedRegionFormatter(out, false, name);
        out << "\n";
    }

    void Formatter::writeSourceInfo(MessageFormatterMap& messageFormatters, std::istream& in, std::ostream& out)
    {
        MessageKey key;
        in.read(reinterpret_cast<char*>(&key), sizeof(MessageKey));

        const auto it = messageFormatters.find(key);
        if (it == messageFormatters.end())
            throw LalError(std::format("Could not find source information {}.", key.key));
        const auto& formatter = it->second;
        out << formatter->getMessage() << "\n";
    }

    void Formatter::writeMessage(MessageFormatterMap& messageFormatters,
                                 std::istream&        in,
                                 std::ostream&        out,
                                 const MessageKey     key,
                                 FormatState&         state,
                                 const bool           order) const
    {
        const auto it = messageFormatters.find(key);
        if (it == messageFormatters.end()) throw LalError(std::format("Could not find message {}.", key.key));

        out << state.getRegionPrepend();

        if (order)
        {
            uint64_t index = 0;
            in.read(reinterpret_cast<char*>(&index), sizeof(uint64_t));
            indexFormatter(out, index);
        }

        const auto& formatter = it->second;
        categoryFormatter(out, formatter->getCategory());
        formatter->format(in, out);
        out << "\n";
    }
}  // namespace lal
