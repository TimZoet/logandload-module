#include "logandload/log/format_type.h"

namespace lal
{
    std::ostream& operator<<(std::ostream& out, const MessageKey& key)
    {
        out << key.key;
        return out;
    }

    bool operator==(const MessageKey& lhs, const MessageKey& rhs) noexcept { return lhs.key == rhs.key; }

    bool operator==(const ParameterKey& lhs, const ParameterKey& rhs) noexcept { return lhs.key == rhs.key; }

    uint32_t hashMessage(const std::string& str)
    {
        auto value = std::numeric_limits<uint32_t>::max();

        // Hash characters in multiples of 4.
        for (size_t i = 0; i < str.size() / 4; i++)
        {
            const auto c0 = static_cast<uint32_t>(str[i * 4 + 0]) << 24;
            const auto c1 = static_cast<uint32_t>(str[i * 4 + 1]) << 16;
            const auto c2 = static_cast<uint32_t>(str[i * 4 + 2]) << 8;
            const auto c3 = static_cast<uint32_t>(str[i * 4 + 3]) << 0;
            value         = value ^ hash(c0 | c1 | c2 | c3);
        }

        // Hash 0-3 remaining characters.
        if (const auto remainder = str.size() % 4; remainder > 0)
        {
            const auto c0 = static_cast<uint32_t>(str[str.size() - remainder]) << 24;
            const auto c1 = remainder > 1 ? static_cast<uint32_t>(str[str.size() - remainder + 1]) << 16 : 0;
            const auto c2 = remainder > 2 ? static_cast<uint32_t>(str[str.size() - remainder + 2]) << 8 : 0;
            value         = value ^ hash(c0 | c1 | c2);
        }

        return value;
    }

    size_t countParameters(const std::string& str) noexcept
    {
        size_t count = 0;

        for (size_t i = 1; i < str.size(); i++)
            if (str[i - 1] == '{' && str[i] == '}') count++;

        return count;
    }

    std::vector<size_t> getParameterIndices(const std::string& s)
    {
        std::vector<size_t> indices;

        for (size_t i = 0, j = s.size() - 1; i < j; i++)
        {
            if (s[i] == '{' && s[i + 1] == '}') indices.push_back(i);
        }

        return indices;
    }
}  // namespace lal
