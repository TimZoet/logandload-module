#pragma once

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/log/format_type.h"

namespace lal
{
    struct Node
    {
        ////////////////////////////////////////////////////////////////
        // Types.
        ////////////////////////////////////////////////////////////////

        enum class Type
        {
            Log     = 0,
            Stream  = 1,
            Region  = 2,
            Message = 3
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
        // ...
        ////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////
        // Member variables.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Node type.
         */
        Type type = Type::Log;

        /**
         * \brief Message key. Only used by named regions and messages.
         */
        MessageKey key;

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
