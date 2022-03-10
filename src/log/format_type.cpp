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
