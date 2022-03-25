#pragma once

////////////////////////////////////////////////////////////////
// Standard includes.
////////////////////////////////////////////////////////////////

#include <atomic>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <semaphore>
#include <stop_token>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

////////////////////////////////////////////////////////////////
// Module includes.
////////////////////////////////////////////////////////////////

#include "common/aligned_alloc.h"

////////////////////////////////////////////////////////////////
// Current target includes.
////////////////////////////////////////////////////////////////

#include "logandload/log/stream.h"
#include "logandload/utils/lal_error.h"

namespace lal
{
    template<is_category_filter C, Ordering Order>
    class Log
    {
    public:
        ////////////////////////////////////////////////////////////////
        // Types.
        ////////////////////////////////////////////////////////////////

        struct FormatType
        {
            std::string               message;
            uint32_t                  category = 0;
            std::vector<ParameterKey> parameters;
        };

        using stream_t   = Stream<C, Order>;
        using category_t = C;
        friend stream_t;

        ////////////////////////////////////////////////////////////////
        // Constructors.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Construct a new Log object. 
         * \param path Path to log file. Format file path is set to log_path + ".fmt".
         * \param globalBufferSize Global buffer size (in bytes).
         */
        Log(std::filesystem::path path, size_t globalBufferSize);

        Log() = delete;

        Log(const Log&) = delete;

        Log(Log&&) noexcept = delete;

        ~Log() noexcept;

        Log& operator=(const Log&) = delete;

        Log& operator=(Log&&) = delete;

        ////////////////////////////////////////////////////////////////
        // Member functions.
        ////////////////////////////////////////////////////////////////

        /**
         * \brief Create a new stream to write to this log.
         * \param size Size of stream buffer in bytes.
         * \return Non-owning pointer to new stream.
         */
        [[nodiscard]] stream_t& createStream(size_t size);

    private:
        /**
         * \brief Flush a stream's back buffer to the log.
         * \param stream Stream.
         */
        void flush(stream_t& stream);

        /**
         * \brief Function that is run in the processor thread to copy queued stream buffers to the global front buffer.
         * \param token Stop token.
         */
        void process(std::stop_token token);

        /**
         * \brief Function that is run in the writer thread to write the back buffer to log file.
         * \param token Stop token.
         */
        void write(std::stop_token token);

        template<typename F, typename... Ts>
        void registerFormat(MessageKey key);

        template<MessageKey K>
        void registerSourceLocation(const std::source_location& loc);

        /**
         * \brief Write all format information to disk.
         */
        void writeFormats();

        ////////////////////////////////////////////////////////////////
        // Member variables.
        ////////////////////////////////////////////////////////////////

        struct
        {
            /**
             * \brief Log file path.
             */
            std::filesystem::path path;

            /**
             * \brief Log file handle.
             */
            std::ofstream file;

            /**
             * \brief List of registered formats.
             */
            std::unordered_map<MessageKey, FormatType> formats;

            /**
             * \brief Mutex for formats.
             */
            std::mutex mutex;

            /**
             * \brief Atomic int used if ordering is enabled.
             */
            std::atomic_uint64_t messageIndex = 0;
        } log;

        struct
        {
            std::vector<std::unique_ptr<stream_t>> streams;

            /**
             * \brief List of all streams that have a back buffer that need to be flushed.
             */
            std::vector<stream_t*> queue;

            /**
             * \brief Mutex for protecting streams and queue.
             */
            std::mutex mutex;
        } streams;

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

        struct
        {
            std::jthread            thread;
            std::condition_variable cv;
            std::atomic_bool        notified;
            std::mutex              mutex;
        } processor;

        struct
        {
            std::binary_semaphore signalSemaphore;
            std::binary_semaphore doneSemaphore;
            std::jthread          thread;
        } writer;
    };

    ////////////////////////////////////////////////////////////////
    // Constructors.
    ////////////////////////////////////////////////////////////////

    template<is_category_filter C, Ordering Order>
    Log<C, Order>::Log(std::filesystem::path path, const size_t globalBufferSize) :
        writer(std::binary_semaphore(0), std ::binary_semaphore(1))
    {
        assert(globalBufferSize > 0);
        buffer.size = globalBufferSize;

        // Open log file.
        log.path = std::move(path);
        log.file = std::ofstream(log.path, std::ios::binary);

        if (!log.file) throw LalError(std::format("Failed to open log file {}", log.path.string()));

        // Create global buffers aligned to 64 bytes.
        buffer.front = static_cast<uint8_t*>(common::aligned_alloc(64, buffer.size));
        buffer.back  = static_cast<uint8_t*>(common::aligned_alloc(64, buffer.size));

        // Start processor thread.
        processor.thread = std::jthread(std::bind_front(&Log::process, this));

        // Start writer thread.
        writer.thread = std::jthread(std::bind_front(&Log::write, this));
    }

    template<is_category_filter C, Ordering Order>
    Log<C, Order>::~Log() noexcept
    {
        // Terminate processor thread.
        processor.thread.request_stop();
        processor.cv.notify_one();
        processor.thread.join();

        // Terminate writer thread.
        writer.thread.request_stop();
        writer.signalSemaphore.release();
        writer.thread.join();

        // Write remaining global front buffer. (Note: the order in which these buffers are written is very relevant.)
        log.file.write(reinterpret_cast<const char*>(buffer.front), buffer.offset);

        // Write remaining back buffers in queue.
        for (auto* stream : streams.queue)
        {
            if (stream->buffer.used > 0)
            {
                log.file.write(reinterpret_cast<const char*>(&stream->index), sizeof stream->index);
                log.file.write(reinterpret_cast<const char*>(&stream->buffer.used), sizeof stream->buffer.used);
                log.file.write(reinterpret_cast<const char*>(stream->buffer.back), stream->buffer.used);
            }
        }

        // Write remaining front buffers of streams to file.
        for (auto& s : streams.streams)
        {
            if (s->buffer.offset > 0)
            {
                log.file.write(reinterpret_cast<const char*>(&s->index), sizeof s->index);
                log.file.write(reinterpret_cast<const char*>(&s->buffer.offset), sizeof s->buffer.offset);
                log.file.write(reinterpret_cast<const char*>(s->buffer.front), s->buffer.offset);
            }
        }

        log.file.close();

        // Write formats file.
        writeFormats();

#ifdef WIN32
        _aligned_free(buffer.front);
        _aligned_free(buffer.back);
#else
        std::free(buffer.front);
        std::free(backBuffer);
#endif
    }

    ////////////////////////////////////////////////////////////////
    // Member functions.
    ////////////////////////////////////////////////////////////////

    template<is_category_filter C, Ordering Order>
    auto Log<C, Order>::createStream(const size_t size) -> stream_t&
    {
        assert(size <= buffer.size);

        std::scoped_lock lock(streams.mutex);
        auto& s = *streams.streams.emplace_back(std::make_unique<stream_t>(*this, streams.streams.size(), size));
        streams.queue.reserve(streams.streams.size());
        return s;
    }

    template<is_category_filter C, Ordering Order>
    void Log<C, Order>::flush(stream_t& stream)
    {
        // Add to queue.
        {
            std::scoped_lock lock(streams.mutex);
            streams.queue.emplace_back(&stream);
        }

        // Notify processor.
        {
            // Note: Deliberately using unique_lock here. condition_variable is not compatible with scoped_lock.
            std::unique_lock lock(processor.mutex);
            processor.notified = true;
        }
        processor.cv.notify_one();
    }

    template<is_category_filter C, Ordering Order>
    void Log<C, Order>::process(const std::stop_token token)
    {
        const auto swap = [this] {
            // Wait to ensure back buffer has been flushed to file.
            writer.doneSemaphore.acquire();

            // Swap buffers.
            std::swap(buffer.front, buffer.back);
            buffer.used   = buffer.offset;
            buffer.offset = 0;

            // Notify writer.
            writer.signalSemaphore.release();
        };

        do {
            // Wait for work.
            std::unique_lock lock(processor.mutex);
            processor.cv.wait(lock, [&] { return processor.notified || token.stop_requested(); });

            // Copy indices of all streams that need to be flushed.
            std::vector<stream_t*> q;
            {
                std::scoped_lock streamsLock(streams.mutex);
                q = streams.queue;
                streams.queue.clear();
            }

            // Wait with unlock until queue was copied.
            processor.notified = false;
            lock.unlock();

            for (auto* stream : q)
            {
                const auto* first = stream->buffer.back;
                const auto* last  = first + stream->buffer.used;

                // Write index of stream and size of block.
                if (buffer.offset + sizeof(size_t) * 2 > buffer.size) swap();
                *reinterpret_cast<size_t*>(buffer.front + buffer.offset)                  = stream->index;
                *reinterpret_cast<size_t*>(buffer.front + buffer.offset + sizeof(size_t)) = stream->buffer.used;
                buffer.offset += sizeof(size_t) * 2;
                if (buffer.offset == buffer.size) swap();

                // We might have to do multiple copies if the front buffer does not have enough space.
                while (first < last)
                {
                    // Calculate how much can be copied this iteration.
                    const auto copySize =
                      std::min(static_cast<size_t>(std::distance(first, last)), buffer.size - buffer.offset);

                    // Copy to front buffer.
                    std::copy(first, first + copySize, buffer.front + buffer.offset);
                    buffer.offset += copySize;
                    first += copySize;

                    // Front buffer is full.
                    if (buffer.offset == buffer.size) swap();
                }

                // Signal to stream that flush is done.
                stream->flushed.release();
            }
        } while (!token.stop_requested());
    }

    template<is_category_filter C, Ordering Order>
    void Log<C, Order>::write(const std::stop_token token)
    {
        do {
            // Wait for work.
            writer.signalSemaphore.acquire();

            // Write back buffer to file.
            if (log.file.good()) log.file.write(reinterpret_cast<const char*>(buffer.back), buffer.used);
            buffer.used = 0;

            // Signal back buffer flush done.
            writer.doneSemaphore.release();
        } while (!token.stop_requested());
    }

    template<is_category_filter C, Ordering Order>
    template<typename F, typename... Ts>
    void Log<C, Order>::registerFormat(const MessageKey key)
    {
        static std::atomic_bool visited(false);

        if (bool b = false; visited.compare_exchange_strong(b, true))
        {
            std::scoped_lock lock(log.mutex);

            // Hash types.
            decltype(FormatType::parameters) types = {hashParameter<Ts>()...};

            // Store format and type information.
            log.formats.try_emplace(key, std::string(F::message), F::category, std::move(types));
        }
    }

    template<is_category_filter C, Ordering Order>
    template<MessageKey K>
    void Log<C, Order>::registerSourceLocation(const std::source_location& loc)
    {
        static std::atomic_bool visited(false);

        if (bool b = false; visited.compare_exchange_strong(b, true))
        {
            std::scoped_lock lock(log.mutex);

            decltype(FormatType::parameters) types = {};

            log.formats.try_emplace(
              K, std::string(loc.file_name()) + std::format("({},{})", loc.line(), loc.column()), 0, types);
        }
    }

    template<is_category_filter C, Ordering Order>
    void Log<C, Order>::writeFormats()
    {
        // Open formats file.
        auto fmtPath = log.path;
        fmtPath += ".fmt";
        auto fmtFile = std::ofstream(fmtPath, std::ios::binary);

        if (!fmtFile) throw LalError(std::format("Failed to open format file {}", fmtPath.string()));

        // Write number of streams.
        auto streamCount = streams.streams.size();
        fmtFile.write(reinterpret_cast<const char*>(&streamCount), sizeof streamCount);

        // Write message order setting.
        fmtFile << (Order == Ordering::Enabled ? static_cast<uint8_t>(1) : static_cast<uint8_t>(0));

        // Write all formats.
        for (const auto& [key, format] : log.formats)
        {
            // Write key.
            fmtFile.write(reinterpret_cast<const char*>(&key), sizeof MessageKey);

            // Write format string.
            const auto length = format.message.size() + 1;
            fmtFile.write(reinterpret_cast<const char*>(&length), sizeof length);
            fmtFile.write(format.message.c_str(), length);

            // Write category.
            fmtFile.write(reinterpret_cast<const char*>(&format.category), sizeof format.category);

            // Write parameter information.
            for (const auto p : format.parameters)
                fmtFile.write(reinterpret_cast<const char*>(&p), sizeof ParameterKey);
        }
    }
}  // namespace lal
