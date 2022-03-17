#pragma once

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/analyze/fmt_type.h"
#include "logandload/log/format_type.h"

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

        Node() = default;

        Node(const Node&) = delete;

        Node(Node&&) = default;

        ~Node() noexcept = default;

        Node& operator=(const Node&) = delete;

        Node& operator=(Node&&) = default;

        ////////////////////////////////////////////////////////////////
        // Getters.
        ////////////////////////////////////////////////////////////////

        [[nodiscard]] size_t getIndex(const Analyzer& analyzer) const noexcept;

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
