#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <concepts>
#include <cstdint>

namespace lal
{
    // clang-format off

    template<typename F>
    concept has_category = requires
    {
        F::category;
        { F::category } -> std::same_as<const uint32_t&>;
    };

    template<typename Category>
    concept is_category_filter = requires
    {
        Category::message;
        Category::region;
        Category::source;
        { Category::template message<void>() } -> std::same_as<bool>;
        { Category::region() } -> std::same_as<bool>;
        { Category::source() } -> std::same_as<bool>;
    };

    // clang-format on

    /**
     * \brief Helper struct that returns false for all format types and special messages, completely disabling logging.
     */
    struct CategoryFilterAll
    {
        template<typename F>
        static consteval bool message()
        {
            return false;
        }

        static consteval bool region() { return false; }

        static consteval bool source() { return false; }
    };

    /**
     * \brief Helper struct that returns true for all format types and special messages, thus not filtering out anything.
     */
    struct CategoryFilterNone
    {
        template<typename F>
        static consteval bool message()
        {
            return true;
        }

        static consteval bool region() { return true; }

        static consteval bool source() { return true; }
    };

    /**
     * \brief Helper struct that returns true for all format types whose category >= the minimum severity. Also returns true for all special messages.
     * \tparam V Minimum severity required to log message.
     */
    template<uint32_t V>
    struct CategoryFilterSeverity
    {
        template<typename F>
        static consteval bool message()
        {
            return F::category >= V;
        }

        static consteval bool region() { return true; }

        static consteval bool source() { return true; }
    };
}  // namespace lal
