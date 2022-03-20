#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <filesystem>
#include <unordered_map>
#include <vector>

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/analyze/fmt_type.h"
#include "logandload/analyze/node.h"
#include "logandload/log/format_type.h"
#include "logandload/utils/lal_error.h"

namespace lal
{
    class Tree;

    class Analyzer
    {
    public:
        ////////////////////////////////////////////////////////////////
        // Constructors.
        ////////////////////////////////////////////////////////////////

        Analyzer();

        Analyzer(const Analyzer&) = delete;

        Analyzer(Analyzer&&) = delete;

        ~Analyzer() noexcept;

        Analyzer& operator=(const Analyzer&) = delete;

        Analyzer& operator=(Analyzer&&) = delete;

        ////////////////////////////////////////////////////////////////
        // Getters.
        ////////////////////////////////////////////////////////////////

        [[nodiscard]] const std::vector<Node>& getNodes() const noexcept;

        [[nodiscard]] size_t getStreamCount() const noexcept;

        ////////////////////////////////////////////////////////////////
        // ...
        ////////////////////////////////////////////////////////////////

        template<typename T>
        void registerParameter()
        {
            static constexpr auto key = hashParameter<T>();
            if (!parameters.try_emplace(key, sizeof(T)).second) throw LalError("Parameter was already registered.");
        }

        ////////////////////////////////////////////////////////////////
        // ...
        ////////////////////////////////////////////////////////////////

        bool read(const std::filesystem::path& path);

        void writeGraph(const std::filesystem::path& path, const Tree* tree = nullptr) const;

    private:
        void readFormatFile(const std::filesystem::path& fmtPath);

        void readLogFile(const std::filesystem::path& path);

        ////////////////////////////////////////////////////////////////
        // Member variables.
        ////////////////////////////////////////////////////////////////

        std::unordered_map<ParameterKey, size_t> parameters;

        std::unordered_map<MessageKey, FormatType> formatTypes;

        size_t streamCount = 0;

        bool messageOrder = false;

        std::vector<std::byte> data;

        std::vector<Node> nodes;
    };
}  // namespace lal
