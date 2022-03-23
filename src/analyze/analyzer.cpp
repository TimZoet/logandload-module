#include "logandload/analyze/analyzer.h"

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <cassert>
#include <format>
#include <fstream>
#include <ranges>

////////////////////////////////////////////////////////////////
// Module includes.
////////////////////////////////////////////////////////////////

#include "common/enum_classes.h"
#include "dot/graph.h"

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/analyze/tree.h"
#include "logandload/utils/lal_error.h"

namespace
{
    struct GroupNode
    {
        lal::MessageKey key;
        size_t          index             = 0;
        size_t          parent            = 0;
        size_t          groupChildCount   = 0;
        size_t          messageChildCount = 0;
    };
}  // namespace

namespace lal
{
    ////////////////////////////////////////////////////////////////
    // Constructors.
    ////////////////////////////////////////////////////////////////

    Analyzer::Analyzer()
    {
        // Register default parameters.
        registerParameter<int8_t>();
        registerParameter<uint8_t>();
        registerParameter<int16_t>();
        registerParameter<uint16_t>();
        registerParameter<int32_t>();
        registerParameter<uint32_t>();
        registerParameter<int64_t>();
        registerParameter<uint64_t>();
        registerParameter<std::byte>();
        registerParameter<float>();
        registerParameter<double>();
        registerParameter<long double>();
    }

    Analyzer::~Analyzer() noexcept = default;

    ////////////////////////////////////////////////////////////////
    // Getters.
    ////////////////////////////////////////////////////////////////

    const std::vector<Node>& Analyzer::getNodes() const noexcept { return nodes; }

    size_t Analyzer::getStreamCount() const noexcept { return nodes[0].childCount; }

    ////////////////////////////////////////////////////////////////
    // ...
    ////////////////////////////////////////////////////////////////

    bool Analyzer::read(const std::filesystem::path& path)
    {
        auto fmtPath = path;
        fmtPath += ".fmt";

        readFormatFile(fmtPath);
        readLogFile(path);

        return true;
    }

    void Analyzer::readFormatFile(const std::filesystem::path& fmtPath)
    {
        // Open formats file.
        auto file = std::ifstream(fmtPath, std::ios::binary | std::ios::ate);
        if (!file) throw LalError(std::format("Failed to open format file {}.", fmtPath.string()));

        const auto length = file.tellg();
        file.seekg(0);

        // Read some settings.
        file.read(reinterpret_cast<char*>(&streamCount), sizeof streamCount);
        {
            int8_t _order = 0;
            file.read(reinterpret_cast<char*>(&_order), sizeof _order);
            if (_order) messageOrder = true;
        }

        // Read list of format types.
        while (file.tellg() != length)
        {
            FormatType formatType;

            // Read message key.
            file.read(reinterpret_cast<char*>(&formatType.key), sizeof(MessageKey));

            // Read format string.
            size_t len;
            file.read(reinterpret_cast<char*>(&len), sizeof(size_t));
            auto* str = new char[len];
            file.read(str, static_cast<std::make_signed_t<size_t>>(len));
            formatType.message = std::string(str);
            delete[] str;

            // Calculate message hash.
            formatType.messageHash = MessageKey{hashMessage(formatType.message)};

            // Read category.
            file.read(reinterpret_cast<char*>(&formatType.category), sizeof formatType.category);

            // Read all parameter keys.
            for (size_t i = 0; i < countParameters(formatType.message); i++)
            {
                ParameterKey paramKey;
                file.read(reinterpret_cast<char*>(&paramKey), sizeof(ParameterKey));

                const auto it = parameters.find(paramKey);
                if (it == parameters.end())
                    throw LalError(std::format("Encountered unregistered parameter {} in format file.", paramKey.key));

                formatType.parameters.emplace_back(paramKey);
                formatType.parameterSize.emplace_back(it->second);
                formatType.messageSize += it->second;
            }

            if (!formatTypes.try_emplace(formatType.key, std::move(formatType)).second)
                throw LalError(std::format("Duplicate message {} in format file.", formatType.key.key));
        }
    }

    void Analyzer::readLogFile(const std::filesystem::path& path)
    {
        {
            // Open log file.
            auto file = std::ifstream(path, std::ios::binary | std::ios::ate);
            if (!file) throw LalError(std::format("Failed to open log file {}.", path.string()));

            // Read whole file into data.
            const auto length = file.tellg();
            file.seekg(0);
            data.resize(length);
            file.read(reinterpret_cast<char*>(data.data()), length);
        }

        /*
         * Do a first pass over the data. Count the total number of messages and regions,
         * as well as per-region number of children. This is stored in the groupNodes.
         * In the second pass, these values can be used to setup the entire tree without
         * reallocations.
         */

        std::vector<GroupNode> groupNodes;
        size_t                 messageCount = 0;
        size_t                 regionCount  = 0;

        {
            for (size_t i = 0; i < streamCount; i++)
            {
                auto& streamNode = groupNodes.emplace_back();
                streamNode.index = i;
            }

            // For each stream, mark stream node as initial active parent node.
            std::vector<size_t> activeParentNode(streamCount);
            for (size_t i = 0; i < streamCount; i++) activeParentNode[i] = i;

            auto pos = data.begin();
            while (pos < data.end())
            {
                // Read block info.
                const auto& streamIndex = reinterpret_cast<size_t&>(*pos);
                pos += sizeof streamIndex;
                const auto& blockSize = reinterpret_cast<size_t&>(*pos);
                pos += sizeof blockSize;

                auto* parentNode = &groupNodes[activeParentNode[streamIndex]];

                // Process block.
                auto blockEnd = pos + static_cast<int64_t>(blockSize);
                while (pos < blockEnd)
                {
                    const auto& key = reinterpret_cast<MessageKey&>(*pos);
                    pos += sizeof key;

                    if (key == MessageTypes::AnonymousRegionStart)
                    {
                        parentNode->groupChildCount++;

                        // Create new node.
                        const auto parentIndex        = parentNode->index;
                        parentNode                    = &groupNodes.emplace_back();
                        parentNode->index             = groupNodes.size() - 1;
                        parentNode->parent            = parentIndex;
                        activeParentNode[streamIndex] = parentNode->index;

                        regionCount++;
                    }
                    else if (key == MessageTypes::NamedRegionStart)
                    {
                        const auto& key2 = reinterpret_cast<MessageKey&>(*pos);
                        pos += sizeof key2;
                        assert(formatTypes.contains(key2));

                        parentNode->groupChildCount++;

                        // Create new node.
                        const auto parentIndex        = parentNode->index;
                        parentNode                    = &groupNodes.emplace_back();
                        parentNode->key               = key2;
                        parentNode->index             = groupNodes.size() - 1;
                        parentNode->parent            = parentIndex;
                        activeParentNode[streamIndex] = parentNode->index;

                        regionCount++;
                    }
                    else if (key == MessageTypes::RegionEnd)
                    {
                        parentNode                    = &groupNodes[parentNode->parent];
                        activeParentNode[streamIndex] = parentNode->index;
                    }
                    else
                    {
                        // Find format type to skip parameter data.
                        const auto it = formatTypes.find(key);
                        assert(it != formatTypes.end());
                        pos += static_cast<int64_t>(it->second.messageSize);

                        // Skip message index.
                        if (messageOrder) pos += static_cast<int64_t>(sizeof(uint64_t));

                        parentNode->messageChildCount++;

                        messageCount++;
                    }
                }
                assert(pos == blockEnd);
            }
            assert(pos == data.end());
        }

        /*
         * Allocate all nodes. 1 for the root, 1 for each stream, and then 1 for each region and message.
         */

        nodes.resize(1 + streamCount + regionCount + messageCount);

        // Initialize root node.
        nodes.front().type       = Node::Type::Log;
        nodes.front().firstChild = nodes.data() + 1;
        nodes.front().childCount = streamCount;

        // Index into groupNodes. Because we traverse the data in the same order,
        // it can simply be incremented each time we encounter a start region message.
        size_t nextGroupIndex = streamCount;

        // Index into nodes. When encountering a start region message, we move it
        // by the number of children in the matching group node. The new node then owns
        // that range of nodes.
        size_t nextIndex = 1 + streamCount;

        // Initialize stream nodes.
        for (size_t i = 0; i < streamCount; i++)
        {
            nodes[i + 1].type   = Node::Type::Stream;
            nodes[i + 1].parent = &nodes.front();

            const auto c = groupNodes[i].groupChildCount + groupNodes[i].messageChildCount;
            if (c)
            {
                nodes[i + 1].firstChild = nodes.data() + nextIndex;
                nextIndex += c;
            }
        }

        /*
         * Do a second pass over the data to initialize all region and message nodes.
         */

        {
            // Mark stream nodes as active parent node.
            std::vector<Node*> activeParentNode(streamCount);
            for (size_t i = 0; i < streamCount; i++) activeParentNode[i] = nodes.data() + i + 1;

            auto pos = data.begin();
            while (pos < data.end())
            {
                // Read block info.
                const auto& streamIndex = reinterpret_cast<size_t&>(*pos);
                pos += sizeof streamIndex;
                const auto& blockSize = reinterpret_cast<size_t&>(*pos);
                pos += sizeof blockSize;

                auto* parentNode = activeParentNode[streamIndex];

                // Process block.
                auto blockEnd = pos + static_cast<int64_t>(blockSize);
                while (pos < blockEnd)
                {
                    const auto& key = reinterpret_cast<MessageKey&>(*pos);
                    pos += sizeof key;

                    if (key == MessageTypes::AnonymousRegionStart)
                    {
                        // Initialize anonymous region node at next position in child node range of parent.
                        auto& node  = *(parentNode->firstChild + parentNode->childCount++);
                        node.type   = Node::Type::Region;
                        node.parent = parentNode;

                        // Assign offset to first child.
                        if (const auto& groupNode = groupNodes[nextGroupIndex++];
                            groupNode.groupChildCount + groupNode.messageChildCount > 0)
                        {
                            node.firstChild = nodes.data() + nextIndex;
                            nextIndex += groupNode.groupChildCount + groupNode.messageChildCount;
                        }

                        // Update parent node for current stream.
                        parentNode                    = &node;
                        activeParentNode[streamIndex] = parentNode;
                    }
                    else if (key == MessageTypes::NamedRegionStart)
                    {
                        const auto& key2 = reinterpret_cast<MessageKey&>(*pos);
                        pos += sizeof key2;
                        const auto it = formatTypes.find(key2);
                        assert(it != formatTypes.end());

                        // Initialize named region node at next position in child node range of parent.
                        auto& node      = *(parentNode->firstChild + parentNode->childCount++);
                        node.type       = Node::Type::Region;
                        node.formatType = &it->second;
                        node.parent     = parentNode;

                        // Assign offset to first child.
                        if (const auto& groupNode = groupNodes[nextGroupIndex++];
                            groupNode.groupChildCount + groupNode.messageChildCount > 0)
                        {
                            node.firstChild = nodes.data() + nextIndex;
                            nextIndex += groupNode.groupChildCount + groupNode.messageChildCount;
                        }

                        // Update parent node for current stream.
                        parentNode                    = &node;
                        activeParentNode[streamIndex] = parentNode;
                    }
                    else if (key == MessageTypes::RegionEnd)
                    {
                        // Update parent node for current stream.
                        parentNode                    = parentNode->parent;
                        activeParentNode[streamIndex] = parentNode;
                    }
                    else
                    {
                        const auto it = formatTypes.find(key);
                        assert(it != formatTypes.end());

                        // Initialize message node at next position in child node range of parent.
                        auto& node      = *(parentNode->firstChild + parentNode->childCount++);
                        node.type       = Node::Type::Message;
                        node.formatType = &it->second;
                        if (messageOrder)
                        {
                            node.index = reinterpret_cast<size_t&>(*pos);
                            pos += static_cast<int64_t>(sizeof(uint64_t));
                        }
                        node.parent = parentNode;

                        // Assign parameter data.
                        const auto size = it->second.messageSize;
                        if (size)
                        {

                            node.data = data.data() + std::distance(data.begin(), pos);
                            pos += static_cast<int64_t>(size);
                        }
                    }
                }
                assert(pos == blockEnd);
            }
            assert(pos == data.end());
        }
    }

    void Analyzer::writeGraph(const std::filesystem::path& path, const Tree* tree) const
    {
        dot::Graph graph;

        std::function<void(dot::Node&, const Node&)> func;
        func = [&](dot::Node& parent, const Node& node) {
            const auto index = node.getIndex(*this);

            //if (tree && !any(tree->getNodes()[index] & Tree::Flags::Enabled))
            //    return;

            auto& childNode = graph.createNode();

            graph.createEdge(parent, childNode);

            if (tree && !any(tree->getNodes()[index] & Tree::Flags::Enabled))
            {
                childNode.attributes["style"]     = "filled";
                childNode.attributes["fillcolor"] = "red";
                return;
            }

            if (node.type == Node::Type::Stream)
            {
                childNode.setLabel("Stream");

                // Traverse children.
                for (size_t i = 0; i < node.childCount; i++) func(childNode, *(node.firstChild + i));
            }
            else if (node.type == Node::Type::Region)
            {
                if (node.formatType && !node.formatType->message.empty())
                    childNode.setLabel(node.formatType->message);
                else
                    childNode.setLabel("Region");

                // Traverse children.
                for (size_t i = 0; i < node.childCount; i++) { func(childNode, *(node.firstChild + i)); }
            }
            else
            {
                childNode.setLabel(node.formatType->message);
            }
        };

        auto& logNode = graph.createNode();
        logNode.setLabel("Log");
        for (size_t i = 1; i <= streamCount; i++) func(logNode, nodes[i]);

        std::ofstream f(path.string());
        graph.write(f);
    }

}  // namespace lal
