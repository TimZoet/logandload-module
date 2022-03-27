#include "logandload/analyze/tree.h"

////////////////////////////////////////////////////////////////
// Module includes.
////////////////////////////////////////////////////////////////

#include "common/enum_classes.h"

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/analyze/analyzer.h"
#include "logandload/utils/lal_error.h"

namespace
{
    /**
     * \brief Get index of child node in list of parent's child nodes.
     * \param child Child node.
     * \param parent Parent node.
     * \return Index.
     */
    [[nodiscard]] size_t getNodeIndex(const lal::Node& child, const lal::Node& parent)
    {
        return static_cast<size_t>(&child - parent.firstChild);
    }

    [[nodiscard]] bool isChildOf(const lal::Node& child, const lal::Node& parent)
    {
        if (&child < parent.firstChild) return false;
        return static_cast<size_t>(&child - parent.firstChild) < parent.childCount;
    }
}  // namespace


namespace lal
{
    ////////////////////////////////////////////////////////////////
    // Constructors.
    ////////////////////////////////////////////////////////////////

    Tree::Tree(Analyzer& a) : analyzer(&a) { nodes.resize(analyzer->getNodes().size(), Flags::Enabled); }

    Tree::~Tree() noexcept = default;

    ////////////////////////////////////////////////////////////////
    // Getters.
    ////////////////////////////////////////////////////////////////

    const std::vector<Tree::Flags>& Tree::getNodes() const noexcept { return nodes; }

    ////////////////////////////////////////////////////////////////
    // Filtering.
    ////////////////////////////////////////////////////////////////

    void Tree::filterStream(const std::function<Flags(Flags, const Node&, size_t)>& f)
    {
        const auto& logNode = analyzer->getNodes().front();
        for (size_t i = 0; i < logNode.childCount; i++) nodes[i + 1] = f(nodes[i + 1], *(logNode.firstChild + i), i);
    }

    void Tree::filterCategory(const std::function<Flags(Flags, uint32_t)>& f)
    {
        traverse(
          [&](const Flags oldFlags, const Node& node) {
              if (node.type == Node::Type::Message) return f(oldFlags, node.formatType->category);
              return oldFlags;
          },
          [](const Flags flags, const Node&) {
              return none(flags & Flags::Enabled) ? Action::Terminate : Action::Apply;
          });
    }

    void Tree::filterCategory(const std::function<Flags(Flags, uint32_t)>&     f,
                              const std::function<Action(Flags, const Node&)>& fAction)
    {
        traverse(
          [&](const Flags oldFlags, const Node& node) {
              if (node.type == Node::Type::Message) return f(oldFlags, node.formatType->category);
              return oldFlags;
          },
          fAction);
    }

    void Tree::filterRegion(const std::function<Flags(Flags, const Node&)>& f)
    {
        traverse(
          [&](const Flags oldFlags, const Node& node) {
              if (node.type == Node::Type::Region) return f(oldFlags, node);
              return oldFlags;
          },
          [](const Flags flags, const Node&) {
              return none(flags & Flags::Enabled) ? Action::Terminate : Action::Apply;
          });
    }

    void Tree::filterRegion(const std::function<Flags(Flags, const Node&)>&  f,
                            const std::function<Action(Flags, const Node&)>& fAction)
    {
        traverse(
          [&](const Flags oldFlags, const Node& node) {
              if (node.type == Node::Type::Region) return f(oldFlags, node);
              return oldFlags;
          },
          fAction);
    }

    void Tree::filterMessageImpl(const MessageKey                                messageHash,
                                 const uint32_t                                  category,
                                 const std::vector<ParameterKey>                 params,
                                 const std::function<Flags(Flags, const Node&)>& f)
    {
        traverse(
          [&](const Flags oldFlags, const Node& node) {
              if (node.type == Node::Type::Message && node.formatType->category == category &&
                  node.formatType->messageHash == messageHash && node.formatType->matches(params))
                  return f(oldFlags, node);
              return oldFlags;
          },
          [](const Flags flags, const Node&) {
              return none(flags & Flags::Enabled) ? Action::Terminate : Action::Apply;
          });
    }

    void Tree::filterMessageImpl(const MessageKey                                 messageHash,
                                 const uint32_t                                   category,
                                 const std::vector<ParameterKey>                  params,
                                 const std::function<Flags(Flags, const Node&)>&  f,
                                 const std::function<Action(Flags, const Node&)>& fAction)
    {
        traverse(
          [&](const Flags oldFlags, const Node& node) {
              if (node.type == Node::Type::Message && node.formatType->category == category &&
                  node.formatType->messageHash == messageHash && node.formatType->matches(params))
                  return f(oldFlags, node);
              return oldFlags;
          },
          fAction);
    }

    void Tree::traverse(const std::function<Flags(Flags, const Node&)>&  f,
                        const std::function<Action(Flags, const Node&)>& fAction)
    {
        const Node* activeNode   = &analyzer->getNodes()[0];
        const Node* previousNode = nullptr;

        while (activeNode)
        {
            // Came here from child. Traverse to next child or to parent if no children left.
            if (previousNode)
            {
                // Traverse to next child.
                const auto nextChildIndex = getNodeIndex(*previousNode, *activeNode) + 1;
                if (nextChildIndex < activeNode->childCount)
                {
                    activeNode   = activeNode->firstChild + nextChildIndex;
                    previousNode = nullptr;
                    continue;
                }

                // Traverse to parent.
                previousNode = activeNode;
                activeNode   = activeNode->parent;
                continue;
            }

            const auto index  = activeNode->getIndex(*analyzer);
            const auto flag   = nodes[index];
            const auto action = fAction(flag, *activeNode);

            // Apply function.
            if (any(action & Action::Apply)) nodes[index] = f(flag, *activeNode);

            // Terminate, traverse back to parent.
            if (any(action & Action::Terminate))
            {
                previousNode = activeNode;
                activeNode   = activeNode->parent;
                continue;
            }

            // Traverse to first child.
            if (activeNode->childCount)
            {
                previousNode = nullptr;
                activeNode   = activeNode->firstChild;
                continue;
            }

            // No children, traverse back to parent.
            previousNode = activeNode;
            activeNode   = activeNode->parent;
        }
    }

    ////////////////////////////////////////////////////////////////
    // ...
    ////////////////////////////////////////////////////////////////

    void Tree::expand(const uint32_t left, const uint32_t right)
    {
        convolution(
          [&](const Node& activeNode, const int32_t i, const size_t firstChildIndex, std::vector<Flags>& newFlags) {
              // Skip already enabled.
              if (any(newFlags[i] & Flags::Enabled)) return;

              // Iterate over siblings on left and right.
              for (int32_t j = i - static_cast<int32_t>(left); j <= i + static_cast<int32_t>(right); j++)
              {
                  // If j in range and sibling is enabled.
                  if (j >= 0 && static_cast<size_t>(j) < activeNode.childCount &&
                      any(nodes[firstChildIndex + j] & Flags::Enabled))
                  {
                      // Enable this node as well.
                      newFlags[i] |= Flags::Enabled;
                      break;
                  }
              }
          });
    }

    void Tree::reduce(const uint32_t left, const uint32_t right)
    {
        convolution(
          [&](const Node& activeNode, const int32_t i, const size_t firstChildIndex, std::vector<Flags>& newFlags) {
              // Skip already disabled.
              if (any(newFlags[i] & Flags::Disabled)) return;

              // Iterate over siblings on left and right.
              for (int32_t j = i - static_cast<int32_t>(left); j <= i + static_cast<int32_t>(right); j++)
              {
                  // If j in range and sibling is disabled.
                  if (j >= 0 && static_cast<size_t>(j) < activeNode.childCount &&
                      none(nodes[firstChildIndex + j] & Flags::Enabled))
                  {
                      // Disable this node as well.
                      newFlags[i] = newFlags[i] & ~Flags::Enabled;
                      break;
                  }
              }
          });
    }

    void Tree::convolution(const std::function<void(const Node&, int32_t, size_t, std::vector<Flags>& newFlags)>& f)
    {
        const Node* activeNode   = &analyzer->getNodes()[0];
        const Node* previousNode = nullptr;

        std::vector<Flags> newFlags;

        while (activeNode)
        {
            // Came here from child. Traverse to next stream/region child or to parent if no region children left.
            if (previousNode)
            {
                // Search for next stream/region child.
                auto nextChildIndex = getNodeIndex(*previousNode, *activeNode);
                do {
                    nextChildIndex++;
                } while (isChildOf(*(activeNode->firstChild + nextChildIndex), *activeNode) &&
                         (activeNode->firstChild + nextChildIndex)->type != Node::Type::Stream &&
                         (activeNode->firstChild + nextChildIndex)->type != Node::Type::Region);

                // Traverse to next stream/region child.
                if (isChildOf(*(activeNode->firstChild + nextChildIndex), *activeNode))
                {
                    activeNode   = activeNode->firstChild + nextChildIndex;
                    previousNode = nullptr;
                    continue;
                }

                // Traverse to parent.
                previousNode = activeNode;
                activeNode   = activeNode->parent;
                continue;
            }

            const auto index = activeNode->getIndex(*analyzer);

            // Terminate, traverse back to parent.
            if (const auto flag = nodes[index]; none(flag & Flags::Enabled))
            {
                previousNode = activeNode;
                activeNode   = activeNode->parent;
                continue;
            }

            // For all child nodes of stream/region nodes, expand selection.
            if (activeNode->type == Node::Type::Stream || activeNode->type == Node::Type::Region)
            {
                // Need to store new flags in a separate array.
                newFlags.resize(activeNode->childCount);

                const auto firstChildIndex = activeNode->firstChild->getIndex(*analyzer);

                // Loop over children.
                for (int32_t i = 0; static_cast<size_t>(i) < activeNode->childCount; i++)
                {
                    newFlags[i] = nodes[firstChildIndex + i];

                    f(*activeNode, i, firstChildIndex, newFlags);
                }

                // Copy new flags.
                std::ranges::copy(newFlags.begin(), newFlags.end(), nodes.begin() + firstChildIndex);
            }

            // Traverse to first child.
            if (activeNode->childCount)
            {
                previousNode = nullptr;
                activeNode   = activeNode->firstChild;
                continue;
            }

            // Traverse back to parent.
            previousNode = activeNode;
            activeNode   = activeNode->parent;
        }
    }

    ////////////////////////////////////////////////////////////////
    // Intersections, unions, etc.
    ////////////////////////////////////////////////////////////////

    Tree& Tree::operator|=(const Tree& rhs)
    {
        if (analyzer != rhs.analyzer) throw LalError("Cannot combine Trees of different Analyzers.");

        for (size_t i = 0; i < nodes.size(); i++) nodes[i] |= rhs.nodes[i] & Flags::Enabled;

        return *this;
    }

    Tree& Tree::operator&=(const Tree& rhs)
    {
        if (analyzer != rhs.analyzer) throw LalError("Cannot combine Trees of different Analyzers.");

        for (size_t i = 0; i < nodes.size(); i++)
            nodes[i] = nodes[i] & ~Flags::Enabled | nodes[i] & rhs.nodes[i] & Flags::Enabled;

        return *this;
    }
}  // namespace lal
