#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <semaphore>
#include <source_location>

////////////////////////////////////////////////////////////////
// Module includes.
////////////////////////////////////////////////////////////////

#include "common/aligned_alloc.h"
#include "common/static_assert.h"

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/log/category.h"
#include "logandload/log/ordering.h"
#include "logandload/log/region.h"

namespace lal
{
    template<is_category_filter C = CategoryFilterNone, Ordering Order = Ordering::Disabled>
    class Log;

    template<is_category_filter C, Ordering Order>
    class Stream
    {
    public:
        ////////////////////////////////////////////////////////////////
        // Types.
        ////////////////////////////////////////////////////////////////

        using log_t            = Log<C, Order>;
        using region_t         = Region<Stream<C, Order>>;
        using movable_region_t = MovableRegion<Stream<C, Order>>;

        friend log_t;
        friend class region_t;
        friend class movable_region_t;

        ////////////////////////////////////////////////////////////////
        // Constructors.
        ////////////////////////////////////////////////////////////////

        Stream(log_t& logger, size_t streamIndex, size_t bufferSize);

        Stream() = delete;

        Stream(const Stream&) = delete;

        Stream(Stream&&) = delete;

        ~Stream() noexcept;

        Stream& operator=(const Stream&) = delete;

        Stream& operator=(Stream&&) = delete;

        ////////////////////////////////////////////////////////////////
        // Logging.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Write a message.
         * \tparam F Format type.
         * \tparam Ts Parameter types. Count must match number of dynamic parameters in format type message.
         * \param values Parameters.
         */
        template<typename F, std::copyable... Ts>
        requires(is_format_type<F, Ts...>) void message(const Ts&... values);

        /**
         * \brief Start a region.
         * \tparam F Format type for a named region. std::nullptr_t for an anonymous region.
         * \return Region (or DisabledRegion if regions are disabled).
         */
        template<typename F = std::nullptr_t>
        auto region();

        /**
         * \brief Start a movable region.
         * \tparam F Format type for a named region. std::nullptr_t for an anonymous region.
         * \return MovableRegion (or DisabledRegion if regions are disabled).
         */
        template<typename F = std::nullptr_t>
        auto movableRegion();

        /**
         * \brief Write a std::source_location.
         * \tparam K Hash of std::source_location.
         * \param loc std::source_location.
         */
        template<uint32_t K>
        void sourceInfo(const std::source_location& loc);

    private:
        void checkFlush(size_t messageSize);

        /**
         * \brief Write a single value to the stream buffer.
         * \tparam T Type.
         * \param value Value.
         * \return *this.
         */
        template<typename T>
        Stream& operator<<(const T& value);

        /**
         * \brief Flush current buffer contents to the log. Assumes buffer is not empty.
         */
        void flush();

        ////////////////////////////////////////////////////////////////
        // Member variables.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Log object.
         */
        log_t* log = nullptr;

        /**
         * \brief Index of stream in log's list of streams.
         */
        size_t index;

        struct
        {
            /**
             * \brief Size of the front and back buffer in bytes.
             */
            size_t size = 0;

            /**
             * \brief Front buffer. Aligned to 64 bytes.
             */
            uint8_t* front = nullptr;

            /**
             * \brief Current offset in the front buffer.
             */
            size_t offset = 0;

            /**
             * \brief Back buffer. Aligned to 64 bytes.
             */
            uint8_t* back = nullptr;

            /**
             * \brief Number of bytes in the back buffer that contain valid data. Always <= size.
             */
            size_t used = 0;
        } buffer;

        /**
         * \brief Semaphore for waiting and signaling flush state.
         */
        std::binary_semaphore flushed;
    };

    ////////////////////////////////////////////////////////////////
    // Constructors.
    ////////////////////////////////////////////////////////////////

    template<is_category_filter C, Ordering Order>
    Stream<C, Order>::Stream(log_t& logger, const size_t streamIndex, const size_t bufferSize) :
        log(&logger), index(streamIndex), flushed(1)
    {
        assert(bufferSize > 0);
        buffer.size = bufferSize;

        // Create stream buffers aligned to 64 bytes.
        buffer.front = static_cast<uint8_t*>(common::aligned_alloc(64, buffer.size));
        buffer.back  = static_cast<uint8_t*>(common::aligned_alloc(64, buffer.size));
    }

    template<is_category_filter C, Ordering Order>
    Stream<C, Order>::~Stream() noexcept
    {
#ifdef WIN32
        _aligned_free(buffer.front);
        _aligned_free(buffer.back);
#else
        std::free(buffer.front);
        std::free(buffer.back);
#endif
    }

    ////////////////////////////////////////////////////////////////
    // Logging.
    ////////////////////////////////////////////////////////////////

    template<is_category_filter C, Ordering Order>
    template<typename F, std::copyable... Ts>
    requires(is_format_type<F, Ts...>) void Stream<C, Order>::message(const Ts&... values)
    {
        // Size of the message in bytes = sizeof(key) + sizeof(parameters...) + sizeof(index).
        static constexpr size_t messageSize =
          (sizeof(MessageKey) + ... + sizeof(Ts)) +
          (Order == Ordering::Enabled ? sizeof(typename decltype(log->log.messageIndex)::value_type) : 0);

        // Log message if category is valid.
        if constexpr (log_t::category_t::template message<F>())
        {
            // Calculate key and register format.
            static constexpr auto key = hashMessage<F, Ts...>();
            log->template registerFormat<F, Ts...>(key);

            checkFlush(messageSize);

            // Write message key.
            *this << key;

            // If ordering is enabled, write unique message index.
            if constexpr (Order == Ordering::Enabled) *this << log->log.messageIndex++;

            // Write values.
            (*this << ... << values);
        }
    }

    template<is_category_filter C, Ordering Order>
    template<typename F>
    auto Stream<C, Order>::region()
    {
        if constexpr (C::template region())
        {
            if constexpr (std::same_as<F, std::nullptr_t>)
                return region_t(*this);
            else if constexpr (is_format_type<F>)
            {
                static constexpr auto key = hashMessage<F>();
                log->template registerFormat<F>(key);
                return region_t(*this, key);
            }
            else
                constexpr_static_assert();
        }
        else
            return DisabledRegion();
    }

    template<is_category_filter C, Ordering Order>
    template<typename F>
    auto Stream<C, Order>::movableRegion()
    {
        if constexpr (C::template region())
        {
            if constexpr (std::same_as<F, std::nullptr_t>)
                return movable_region_t(*this);
            else if constexpr (is_format_type<F>)
            {
                static constexpr auto key = hashMessage<F>();
                log->template registerFormat<F>(key);
                return movable_region_t(*this, key);
            }
            else
                constexpr_static_assert();
        }
        else
            return DisabledRegion();
    }

    template<is_category_filter C, Ordering Order>
    template<uint32_t K>
    void Stream<C, Order>::sourceInfo(const std::source_location& loc)
    {
        static constexpr auto K2 = MessageKey{K};
        if constexpr (C::template source())
        {
            static constexpr size_t messageSize = sizeof(MessageKey);
            checkFlush(messageSize);

            log->registerSourceLocation<K2>(loc);
            *this << K2;
        }
    }

    template<is_category_filter C, Ordering Order>
    void Stream<C, Order>::checkFlush(const size_t messageSize)
    {
        assert(messageSize <= buffer.size);

        // Buffer would overflow when writing this message.
        if (messageSize + buffer.offset > buffer.size) flush();
    }

    template<is_category_filter C, Ordering Order>
    template<typename T>
    Stream<C, Order>& Stream<C, Order>::operator<<(const T& value)
    {
        assert(sizeof(T) <= buffer.size && sizeof(T) + buffer.offset <= buffer.size);

        // Write value to buffer.
        reinterpret_cast<T&>(buffer.front[buffer.offset]) = value;
        buffer.offset += sizeof(T);

        return *this;
    }

    template<is_category_filter C, Ordering Order>
    void Stream<C, Order>::flush()
    {
        // Wait to ensure back buffer has been flushed to log's front buffer.
        flushed.acquire();

        // Swap buffers.
        std::swap(buffer.front, buffer.back);
        buffer.used   = buffer.offset;
        buffer.offset = 0;

        // Flush buffer.
        log->flush(*this);
    }
}  // namespace lal
