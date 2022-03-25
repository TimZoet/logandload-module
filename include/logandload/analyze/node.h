#pragma once

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/analyze/fmt_type.h"
#include "logandload/log/format_type.h"
#include "logandload/utils/lal_error.h"

namespace lal
{
    class Analyzer;

    struct Node
    {
        ////////////////////////////////////////////////////////////////
        // Types.
        ////////////////////////////////////////////////////////////////

        enum class Type
        {
            Log     = 1,
            Stream  = 2,
            Region  = 4,
            Message = 8
        };

        ////////////////////////////////////////////////////////////////
        // Constructors.
        ////////////////////////////////////////////////////////////////

        Node();

        Node(const Node&) = delete;

        Node(Node&&) noexcept;

        ~Node() noexcept;

        Node& operator=(const Node&) = delete;

        Node& operator=(Node&&) noexcept;

        ////////////////////////////////////////////////////////////////
        // Getters.
        ////////////////////////////////////////////////////////////////

        [[nodiscard]] size_t getIndex(const Analyzer& analyzer) const noexcept;

        /**
         * \brief Returns whether this node holds a parameter of the given type at the given index.
         * \tparam T Parameter.
         * \param index Parameter index.
         * \return True or false.
         */
        template<typename T>
        [[nodiscard]] bool has(const size_t index) const
        {
            static constexpr auto key = hashParameter<T>();
            if (index >= formatType->parameters.size()) throw LalError("Parameter index is out of range.");
            return formatType->parameters[index] == key;
        }

        /**
         * \brief Get the value of a parameter. Node should have a parameter of the given type at the given index.
         * \tparam T Parameter type.
         * \param index Parameter index.
         * \return Value.
         */
        template<typename T>
        [[nodiscard]] const T& get(const size_t index) const
        {
            if (!has<T>(index)) throw LalError("Parameter type does not match.");

            // Sum size of preceding parameters.
            size_t offset = 0;
            for (size_t i = 0; i < index; i++) offset += formatType->parameterSize[i];

            return *reinterpret_cast<const T*>(data + offset);
        }

        ////////////////////////////////////////////////////////////////
        // Member variables.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Node type.
         */
        Type type = Type::Log;

        /**
         * \brief Format type.
         */
        FormatType* formatType = nullptr;

        /**
         * \brief Unique ordered index. Only used by messages if message ordering was enabled.
         */
        size_t index = 0;

        /**
         * \brief Parent node.
         */
        Node* parent = nullptr;

        /**
         * \brief First child node.
         */
        Node* firstChild = nullptr;

        /**
         * \brief Number of children.
         */
        size_t childCount = 0;

        /**
         * \brief Pointer to parameter data.
         */
        std::byte* data = nullptr;
    };
}  // namespace lal
