#pragma once

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/log/format_type.h"

namespace lal
{
    template<typename S>
    class Region
    {
        using stream_t = S;
        stream_t& s;

    public:
        Region() = delete;

        explicit Region(stream_t& s) : s(s)
        {
            static constexpr size_t messageSize = sizeof(MessageKey);
            s.checkFlush(messageSize);
            s << MessageTypes::AnonymousRegionStart;
        }

        Region(stream_t& s, const MessageKey key) : s(s)
        {
            static constexpr size_t messageSize = sizeof(MessageKey) * 2;
            s.checkFlush(messageSize);
            s << MessageTypes::NamedRegionStart << key;
        }

        Region(const Region&) = delete;

        Region(Region&&) = delete;

        ~Region() noexcept
        {
            static constexpr size_t messageSize = sizeof(MessageKey);
            s.checkFlush(messageSize);
            s << MessageTypes::RegionEnd;
        }

        Region& operator=(const Region&) = delete;

        Region& operator=(Region&&) = delete;
    };

    template<typename S>
    class MovableRegion
    {
        using stream_t = S;
        stream_t& s;
        bool      moved = false;

    public:
        MovableRegion() = delete;

        explicit MovableRegion(stream_t& s) : s(s)
        {
            static constexpr size_t messageSize = sizeof(MessageKey);
            s.checkFlush(messageSize);
            s << MessageTypes::AnonymousRegionStart;
        }

        MovableRegion(stream_t& s, const MessageKey key) : s(s)
        {
            static constexpr size_t messageSize = sizeof(MessageKey) * 2;
            s.checkFlush(messageSize);
            s << MessageTypes::NamedRegionStart << key;
        }

        MovableRegion(const MovableRegion&) = delete;

        MovableRegion(MovableRegion&& other) noexcept : s(other.s) { other.moved = true; }

        ~MovableRegion() noexcept
        {
            if (!moved)
            {
                static constexpr size_t messageSize = sizeof(MessageKey);
                s.checkFlush(messageSize);
                s << MessageTypes::RegionEnd;
            }
        }

        MovableRegion& operator=(const MovableRegion&) = delete;

        MovableRegion& operator=(MovableRegion&& other) noexcept
        {
            other.moved = true;
            s           = other.s;
            return *this;
        }
    };

    class DisabledRegion
    {
    };
}  // namespace lal
