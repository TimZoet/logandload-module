#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <source_location>
#include <vector>

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/log/category.h"

namespace lal
{
    struct MessageKey
    {
        uint32_t key = 0;
    };

    struct ParameterKey
    {
        uint32_t key = 0;
    };

    std::ostream& operator<<(std::ostream& out, const MessageKey& key);

    bool operator==(const MessageKey& lhs, const MessageKey& rhs) noexcept;

    bool operator==(const ParameterKey& lhs, const ParameterKey& rhs) noexcept;

    struct MessageTypes
    {
        static constexpr MessageKey AnonymousRegionStart = {0};
        static constexpr MessageKey NamedRegionStart     = {1};
        static constexpr MessageKey RegionEnd            = {2};
    };

    /**
     * \brief Hash an uint32_t. (Thomas Wang, Jan 1997)
     * \param s Value.
     * \return Hash.
     */
    [[nodiscard]] constexpr uint32_t hash(const uint32_t& s) noexcept
    {
        const auto a = s ^ 61 ^ s >> 16;
        const auto b = a * 9;
        const auto c = b ^ b >> 4;
        const auto d = c * 0x27d4eb2d;
        const auto e = d ^ d >> 15;
        return e;
    }

    /**
     * \brief Hash a static string.
     * \tparam N String length.
     * \param str String.
     * \return Hash.
     */
    template<size_t N>
    [[nodiscard]] consteval uint32_t hash(const char (&str)[N]) noexcept
    {
        auto value = std::numeric_limits<uint32_t>::max();

        // Hash characters in multiples of 4.
        for (size_t i = 0; i < N / 4; i++)
        {
            const auto c0 = static_cast<uint32_t>(*(str + i * 4 + 0)) << 24;
            const auto c1 = static_cast<uint32_t>(*(str + i * 4 + 1)) << 16;
            const auto c2 = static_cast<uint32_t>(*(str + i * 4 + 2)) << 8;
            const auto c3 = static_cast<uint32_t>(*(str + i * 4 + 3)) << 0;
            value         = value ^ hash(c0 | c1 | c2 | c3);
        }

        // Hash 0-3 remaining characters.
        const auto remainder = N % 4;
        if constexpr (remainder > 0)
        {
            const auto c0 = static_cast<uint32_t>(*(str + N - remainder)) << 24;
            const auto c1 = remainder > 1 ? static_cast<uint32_t>(*(str + N - remainder + 1)) << 16 : 0;
            const auto c2 = remainder > 2 ? static_cast<uint32_t>(*(str + N - remainder + 2)) << 8 : 0;
            value         = value ^ hash(c0 | c1 | c2);
        }

        return value;
    }

    /**
     * \brief Hash a static string.
     * \param str String.
     * \return Hash.
     */
    consteval uint32_t hash(const char* str)
    {
        size_t N = 0;
        while (str[N] != '\0') N++;

        auto value = std::numeric_limits<uint32_t>::max();

        // Hash characters in multiples of 4.
        for (size_t i = 0; i < N / 4; i++)
        {
            const auto c0 = static_cast<uint32_t>(*(str + i * 4 + 0)) << 24;
            const auto c1 = static_cast<uint32_t>(*(str + i * 4 + 1)) << 16;
            const auto c2 = static_cast<uint32_t>(*(str + i * 4 + 2)) << 8;
            const auto c3 = static_cast<uint32_t>(*(str + i * 4 + 3)) << 0;
            value         = value ^ hash(c0 | c1 | c2 | c3);
        }

        // Hash 0-3 remaining characters.
        if (const auto remainder = N % 4; remainder > 0)
        {
            const auto c0 = static_cast<uint32_t>(*(str + N - remainder)) << 24;
            const auto c1 = remainder > 1 ? static_cast<uint32_t>(*(str + N - remainder + 1)) << 16 : 0;
            const auto c2 = remainder > 2 ? static_cast<uint32_t>(*(str + N - remainder + 2)) << 8 : 0;
            value         = value ^ hash(c0 | c1 | c2);
        }

        return value;
    }

    /**
     * \brief Hash a string.
     * \param str String.
     * \return Hash.
     */
    uint32_t hashMessage(const std::string& str);

    /**
     * \brief Hash a std::source_location.
     * \param loc Location.
     * \return Hash.
     */
    consteval uint32_t hash(const std::source_location& loc)
    {
        return hash(loc.file_name()) ^ hash(loc.line()) ^ hash(loc.column());
    }

    /**
     * \brief Hash a type name.
     * \tparam T Type.
     * \return Hash.
     */
    template<typename T>
    [[nodiscard]] constexpr ParameterKey hashParameter() noexcept
    {
#if defined _MSC_VER
        return ParameterKey{hash(__FUNCSIG__)};
#elif defined __clang__ || (defined __GNUC__)
        return ParameterKey{hash(__PRETTY_FUNCTION__)};
#endif
        // Unfortunately, source_location::function_name() completely ignores any template parameters. Adding it as a defaulted parameter causes other weird issues.
        //constexpr auto loc = std::source_location::current();
        //return ParameterKey{hash(loc)};
    }

    template<>
    [[nodiscard]] constexpr ParameterKey hashParameter<std::nullptr_t>() noexcept
    {
        return ParameterKey{0};
    }

    /**
     * \brief Counts the number of dynamic parameters in the given string. Each occurrence of '{}' indicates a parameter.
     * \tparam N String length.
     * \param str String.
     * \return Number of dynamic parameters in string.
     */
    template<size_t N>
    [[nodiscard]] consteval size_t countParameters(const char (&str)[N]) noexcept
    {
        size_t count = 0;

        for (size_t i = 1; i < N; i++)
            if (str[i - 1] == '{' && str[i] == '}') count++;

        return count;
    }

    // clang-format off
    
    template<typename F>
    concept has_message = requires
    {
        F::message;
        { F::message } -> std::convertible_to<std::string>;
        { F::message } -> std::convertible_to<const char*>;
        //{ F::message } -> std::same_as<const char[]>; // Cannot use this same_as. Why?
    };

    template<typename F, typename... Ts>
    concept is_format_type = has_message<F> && has_category<F> && sizeof...(Ts) == countParameters(F::message);

    // clang-format on

    /**
     * \brief Calculate the hash of a format type and a list of dynamic parameter types, to be used as a unique identifier of a message type.
     * \tparam F Format type.
     * \tparam Ts Dynamic parameter types.
     * \return Hash.
     */
    template<typename F, typename... Ts>
    requires is_format_type<F, Ts...>
    [[nodiscard]] consteval MessageKey hashMessage() noexcept
    {
        return MessageKey{((hash(F::message) ^ hash(F::category)) ^ ... ^ hashParameter<Ts>().key)};
    }

    /**
     * \brief Counts the number of dynamic parameters in the given string. Each occurrence of '{}' indicates a parameter.
     * \param str String.
     * \return Number of dynamic parameters in string.
     */
    [[nodiscard]] size_t countParameters(const std::string& str) noexcept;

    /**
     * \brief Get the offsets of all dynamic parameters (indicated by '{}') in the given string.
     * \param s String.
     * \return Offsets.
     */
    [[nodiscard]] std::vector<size_t> getParameterIndices(const std::string& s);
}  // namespace lal

namespace std
{
    template<>
    struct hash<lal::MessageKey>
    {
        size_t operator()(const lal::MessageKey& k) const noexcept { return std::hash<decltype(k.key)>{}(k.key); }
    };

    template<>
    struct hash<lal::ParameterKey>
    {
        size_t operator()(const lal::ParameterKey& k) const noexcept { return std::hash<decltype(k.key)>{}(k.key); }
    };
}  // namespace std
