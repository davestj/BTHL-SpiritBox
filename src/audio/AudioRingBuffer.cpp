/**
 * @file src/audio/AudioRingBuffer.cpp
 * @title AudioRingBuffer Implementation
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We implement the lock-free ring buffer for real-time audio streaming.
 * @reason Lock-free is essential for glitch-free audio between the SDR capture thread
 *         and the PortAudio callback thread.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation
 */

#include "audio/AudioRingBuffer.h"

namespace bthl::spiritbox {

AudioRingBuffer::AudioRingBuffer(size_t capacity) {
    size_t powerOfTwo = nextPowerOfTwo(capacity);
    m_buffer.resize(powerOfTwo, 0.0f);
    m_mask = powerOfTwo - 1;
}

size_t AudioRingBuffer::write(const float* data, size_t count) {
    size_t available = availableWrite();
    size_t toWrite = std::min(count, available);

    size_t writePos = m_writePos.load(std::memory_order_relaxed);

    for (size_t i = 0; i < toWrite; ++i) {
        m_buffer[(writePos + i) & m_mask] = data[i];
    }

    m_writePos.store(writePos + toWrite, std::memory_order_release);
    return toWrite;
}

size_t AudioRingBuffer::write(const std::vector<float>& data) {
    return write(data.data(), data.size());
}

size_t AudioRingBuffer::read(float* data, size_t count) {
    size_t available = availableRead();
    size_t toRead = std::min(count, available);

    size_t readPos = m_readPos.load(std::memory_order_relaxed);

    for (size_t i = 0; i < toRead; ++i) {
        data[i] = m_buffer[(readPos + i) & m_mask];
    }

    m_readPos.store(readPos + toRead, std::memory_order_release);
    return toRead;
}

size_t AudioRingBuffer::read(std::vector<float>& data, size_t count) {
    data.resize(count);
    size_t actualRead = read(data.data(), count);
    data.resize(actualRead);
    return actualRead;
}

size_t AudioRingBuffer::peek(float* data, size_t count) const {
    size_t available = availableRead();
    size_t toPeek = std::min(count, available);
    size_t readPos = m_readPos.load(std::memory_order_acquire);

    for (size_t i = 0; i < toPeek; ++i) {
        data[i] = m_buffer[(readPos + i) & m_mask];
    }

    return toPeek;
}

size_t AudioRingBuffer::availableRead() const {
    return m_writePos.load(std::memory_order_acquire) - m_readPos.load(std::memory_order_acquire);
}

size_t AudioRingBuffer::availableWrite() const {
    return (m_mask + 1) - availableRead();
}

void AudioRingBuffer::clear() {
    m_readPos.store(0, std::memory_order_release);
    m_writePos.store(0, std::memory_order_release);
}

size_t AudioRingBuffer::capacity() const {
    return m_mask + 1;
}

bool AudioRingBuffer::isEmpty() const {
    return availableRead() == 0;
}

bool AudioRingBuffer::isFull() const {
    return availableWrite() == 0;
}

size_t AudioRingBuffer::nextPowerOfTwo(size_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

} // namespace bthl::spiritbox
