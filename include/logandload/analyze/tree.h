#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <functional>
#include <vector>

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/analyze/node.h"

namespace lal
{
    class Analyzer;

    class Tree
    {
    public:
        ////////////////////////////////////////////////////////////////
        // Types.
        ////////////////////////////////////////////////////////////////

        enum class Flags : uint8_t
        {
            Disabled = 0,
            Enabled  = 1,
        };

        ////////////////////////////////////////////////////////////////
        // Constructors.
        ////////////////////////////////////////////////////////////////

        Tree() = delete;

        explicit Tree(Analyzer& a);

        Tree(const Tree&) = delete;

        Tree(Tree&&) = delete;

        ~Tree() noexcept;

        Tree& operator=(const Tree&) = delete;

        Tree& operator=(Tree&&) = delete;

        ////////////////////////////////////////////////////////////////
        // Getters.
        ////////////////////////////////////////////////////////////////

        [[nodiscard]] const std::vector<Flags>& getNodes() const noexcept;

        ////////////////////////////////////////////////////////////////
        // Filtering.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Filter stream nodes.
         * \param f Function to apply to stream nodes. First parameter is old flags. Second parameter is stream node. Third parameter is stream index. Returns new flags.
         */
        void filterStream(std::function<Flags(Flags, const Node&, size_t)> f);

        /**
         * \brief Filter message nodes by category.
         * \param f Function to apply to message nodes. First parameter is old flags. Second parameter is category. Returns new flags.
         */
        void filterCategory(std::function<Flags(Flags, uint32_t)> f);

        /**
         * \brief Filter region nodes.
         * \param f Function to apply to region nodes. First parameter is old flags. Second parameter is region node. Returns new flags.
         */
        void filterRegion(std::function<Flags(Flags, const Node&)> f);

        ////////////////////////////////////////////////////////////////
        // Expand/Reduce.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Enable disabled nodes around already enabled nodes. (Does not apply to stream nodes.)
         * \param left Number of nodes to the left of each enabled node to enable as well.
         * \param right Number of nodes to the right of each enabled node to enable as well.
         */
        void expand(uint32_t left, uint32_t right);

        /**
         * \brief Disable nodes if they are not surrounded by enabled nodes. (Does not apply to stream nodes.)
         * \param left Number of nodes to the left of each enabled node that need to be enabled as well to prevent it from being disabled.
         * \param right Number of nodes to the right of each enabled node that need to be enabled as well to prevent it from being disabled.
         */
        void reduce(uint32_t left, uint32_t right);

        ////////////////////////////////////////////////////////////////
        // Intersections.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Enable all nodes that are already enabled in either tree. Disable all others.
         * \param rhs Other tree.
         * \return *this.
         */
        Tree& operator|=(const Tree& rhs);

        /**
         * \brief Enable all nodes that are already enabled in both trees. Disable all others.
         * \param rhs Other tree.
         * \return *this.
         */
        Tree& operator&=(const Tree& rhs);

    private:
        void traverse(std::function<Flags(Flags, const Node&)> f);

        void convolution(const std::function<void(const Node&, int32_t, size_t, std::vector<Flags>& newFlags)>& f);

        ////////////////////////////////////////////////////////////////
        // Member variables.
        ////////////////////////////////////////////////////////////////

        Analyzer* analyzer = nullptr;

        std::vector<Flags> nodes;
    };
}  // namespace lal
