/**
 * @file src/audio/AudioRingBuffer.h
 * @title AudioRingBuffer - Lock-Free Circular Audio Buffer
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We provide a thread-safe, lock-free ring buffer for passing audio samples
 *          between the demodulation thread and the audio output/detection threads.
 * @reason Real-time audio requires lock-free data structures to avoid priority inversion
 *         and ensure consistent latency between producer and consumer threads.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with atomic read/write positions
 */

#ifndef BTHL_SPIRITBOX_AUDIO_RING_BUFFER_H
#define BTHL_SPIRITBOX_AUDIO_RING_BUFFER_H

#include <vector>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <algorithm>

namespace bthl::spiritbox {

/**
 * @class AudioRingBuffer
 * @purpose We implement a single-producer, single-consumer lock-free ring buffer
 *          for float audio samples. The buffer size must be a power of two for
 *          efficient modulo operations via bitmask.
 */
class AudioRingBuffer {
public:
    /**
     * @brief We construct a ring buffer with the specified capacity
     * @param capacity Buffer size in samples (will be rounded up to next power of two)
     */
    explicit AudioRingBuffer(size_t capacity = 65536);

    /**
     * @brief We write audio samples into the ring buffer (producer side)
     * @param data Pointer to source samples
     * @param count Number of samples to write
     * @return Number of samples actually written (may be less if buffer is full)
     */
    size_t write(const float* data, size_t count);

    /**
     * @brief We write a vector of audio samples into the ring buffer
     */
    size_t write(const std::vector<float>& data);

    /**
     * @brief We read audio samples from the ring buffer (consumer side)
     * @param data Pointer to destination buffer
     * @param count Number of samples to read
     * @return Number of samples actually read (may be less if buffer is empty)
     */
    size_t read(float* data, size_t count);

    /**
     * @brief We read audio samples into a vector
     */
    size_t read(std::vector<float>& data, size_t count);

    /**
     * @brief We peek at samples without advancing the read position
     */
    size_t peek(float* data, size_t count) const;

    /**
     * @brief We check how many samples are available for reading
     */
    [[nodiscard]] size_t availableRead() const;

    /**
     * @brief We check how many samples can be written
     */
    [[nodiscard]] size_t availableWrite() const;

    /**
     * @brief We reset the buffer to empty state
     */
    void clear();

    /**
     * @brief We return the total buffer capacity in samples
     */
    [[nodiscard]] size_t capacity() const;

    /**
     * @brief We check if the buffer is empty
     */
    [[nodiscard]] bool isEmpty() const;

    /**
     * @brief We check if the buffer is full
     */
    [[nodiscard]] bool isFull() const;

private:
    /**
     * @brief We round up to the next power of two for efficient masking
     */
    static size_t nextPowerOfTwo(size_t v);

    std::vector<float> m_buffer;
    size_t m_mask;
    std::atomic<size_t> m_writePos{0};
    std::atomic<size_t> m_readPos{0};
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_AUDIO_RING_BUFFER_H
