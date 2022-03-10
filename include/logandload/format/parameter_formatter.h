#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/log/format_type.h"

namespace lal
{
    struct IParameterFormatter
    {
        ////////////////////////////////////////////////////////////////
        // Constructors.
        ////////////////////////////////////////////////////////////////

        IParameterFormatter() = default;

        IParameterFormatter(const IParameterFormatter&) = default;

        IParameterFormatter(IParameterFormatter&&) = default;

        virtual ~IParameterFormatter() noexcept = default;

        IParameterFormatter& operator=(const IParameterFormatter&) = default;

        IParameterFormatter& operator=(IParameterFormatter&&) = default;

        ////////////////////////////////////////////////////////////////
        // Format.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Size of parameter in bytes.
         * \return Size.
         */
        [[nodiscard]] virtual size_t size() const noexcept = 0;

        /**
         * \brief Read parameter from input and write formatted value to output.
         * \param in Input stream.
         * \param out Output stream.
         */
        virtual void format(std::istream& in, std::ostream& out) const = 0;
    };

    using IParameterFormatterPtr = std::unique_ptr<IParameterFormatter>;
    using ParameterFormatterMap  = std::unordered_map<ParameterKey, IParameterFormatterPtr>;

    template<typename T>
    struct ParameterFormatter final : IParameterFormatter
    {
        using type                        = T;
        using func_t                      = std::function<void(std::ostream&, const type&)>;
        static constexpr ParameterKey key = hashParameter<T>();
        func_t                        func;

        ////////////////////////////////////////////////////////////////
        // Constructors.
        ////////////////////////////////////////////////////////////////

        ParameterFormatter() = delete;

        explicit ParameterFormatter(func_t f) : func(std::move(f)) {}

        ParameterFormatter(const ParameterFormatter&) = default;

        ParameterFormatter(ParameterFormatter&&) = default;

        ~ParameterFormatter() noexcept override = default;

        ParameterFormatter& operator=(const ParameterFormatter&) = default;

        ParameterFormatter& operator=(ParameterFormatter&&) = default;

        ////////////////////////////////////////////////////////////////
        // Format.
        ////////////////////////////////////////////////////////////////

        [[nodiscard]] size_t size() const noexcept override { return sizeof(type); }

        void format(std::istream& in, std::ostream& out) const override
        {
            type value;
            in.read(reinterpret_cast<char*>(&value), size());
            func(out, value);
        }
    };
}  // namespace lal
