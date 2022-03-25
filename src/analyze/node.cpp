#include "logandload/analyze/node.h"

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/analyze/analyzer.h"

namespace lal
{
    ////////////////////////////////////////////////////////////////
    // Constructors.
    ////////////////////////////////////////////////////////////////

    Node::Node() = default;

    Node::Node(Node&&) noexcept = default;

    Node::~Node() noexcept = default;

    Node& Node::operator=(Node&&) noexcept = default;

    ////////////////////////////////////////////////////////////////
    // Getters.
    ////////////////////////////////////////////////////////////////

    size_t Node::getIndex(const Analyzer& analyzer) const noexcept
    {
        return std::distance(analyzer.getNodes().data(), this);
    }

}  // namespace lal
