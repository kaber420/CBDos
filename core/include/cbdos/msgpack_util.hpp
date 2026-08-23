#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

namespace cbdos {
namespace msgpack {

// ────────────────────────────────────────────────────────────────
//  MsgPackWriter: Serializador binario MessagePack en memoria
// ────────────────────────────────────────────────────────────────
class MsgPackWriter {
public:
    MsgPackWriter() {
        m_buffer.reserve(512);
    }

    const std::vector<uint8_t>& getBuffer() const { return m_buffer; }
    std::string toString() const {
        return std::string(reinterpret_cast<const char*>(m_buffer.data()), m_buffer.size());
    }
    void clear() { m_buffer.clear(); }

    void writeNil() {
        m_buffer.push_back(0xc0);
    }

    void writeBool(bool val) {
        m_buffer.push_back(val ? 0xc3 : 0xc2);
    }

    void writeInt(int32_t val) {
        if (val >= 0 && val <= 127) {
            m_buffer.push_back((uint8_t)val);
        } else if (val >= -32 && val < 0) {
            m_buffer.push_back((uint8_t)val);
        } else if (val >= -128 && val <= 127) {
            m_buffer.push_back(0xd0);
            m_buffer.push_back((uint8_t)(int8_t)val);
        } else if (val >= -32768 && val <= 32767) {
            m_buffer.push_back(0xd1);
            m_buffer.push_back((uint8_t)((val >> 8) & 0xFF));
            m_buffer.push_back((uint8_t)(val & 0xFF));
        } else {
            m_buffer.push_back(0xd2);
            m_buffer.push_back((uint8_t)((val >> 24) & 0xFF));
            m_buffer.push_back((uint8_t)((val >> 16) & 0xFF));
            m_buffer.push_back((uint8_t)((val >> 8) & 0xFF));
            m_buffer.push_back((uint8_t)(val & 0xFF));
        }
    }

    void writeString(const std::string& str) {
        size_t len = str.length();
        if (len <= 31) {
            m_buffer.push_back((uint8_t)(0xa0 | len));
        } else if (len <= 255) {
            m_buffer.push_back(0xd9);
            m_buffer.push_back((uint8_t)len);
        } else if (len <= 65535) {
            m_buffer.push_back(0xda);
            m_buffer.push_back((uint8_t)((len >> 8) & 0xFF));
            m_buffer.push_back((uint8_t)(len & 0xFF));
        } else {
            m_buffer.push_back(0xdb);
            m_buffer.push_back((uint8_t)((len >> 24) & 0xFF));
            m_buffer.push_back((uint8_t)((len >> 16) & 0xFF));
            m_buffer.push_back((uint8_t)((len >> 8) & 0xFF));
            m_buffer.push_back((uint8_t)(len & 0xFF));
        }
        if (len > 0) {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(str.data());
            m_buffer.insert(m_buffer.end(), p, p + len);
        }
    }

    void writeArrayHeader(size_t size) {
        if (size <= 15) {
            m_buffer.push_back((uint8_t)(0x90 | size));
        } else if (size <= 65535) {
            m_buffer.push_back(0xdc);
            m_buffer.push_back((uint8_t)((size >> 8) & 0xFF));
            m_buffer.push_back((uint8_t)(size & 0xFF));
        } else {
            m_buffer.push_back(0xdd);
            m_buffer.push_back((uint8_t)((size >> 24) & 0xFF));
            m_buffer.push_back((uint8_t)((size >> 16) & 0xFF));
            m_buffer.push_back((uint8_t)((size >> 8) & 0xFF));
            m_buffer.push_back((uint8_t)(size & 0xFF));
        }
    }

    void writeMapHeader(size_t size) {
        if (size <= 15) {
            m_buffer.push_back((uint8_t)(0x80 | size));
        } else if (size <= 65535) {
            m_buffer.push_back(0xde);
            m_buffer.push_back((uint8_t)((size >> 8) & 0xFF));
            m_buffer.push_back((uint8_t)(size & 0xFF));
        } else {
            m_buffer.push_back(0xdf);
            m_buffer.push_back((uint8_t)((size >> 24) & 0xFF));
            m_buffer.push_back((uint8_t)((size >> 16) & 0xFF));
            m_buffer.push_back((uint8_t)((size >> 8) & 0xFF));
            m_buffer.push_back((uint8_t)(size & 0xFF));
        }
    }

private:
    std::vector<uint8_t> m_buffer;
};

// ────────────────────────────────────────────────────────────────
//  MsgPackReader: Deserializador seguro con parser zero-alloc
// ────────────────────────────────────────────────────────────────
class MsgPackReader {
public:
    MsgPackReader(const uint8_t* data, size_t size)
        : m_data(data), m_size(size), m_offset(0) {}

    MsgPackReader(const std::string& str)
        : m_data(reinterpret_cast<const uint8_t*>(str.data())), m_size(str.size()), m_offset(0) {}

    size_t getOffset() const { return m_offset; }
    size_t getRemaining() const { return (m_offset < m_size) ? (m_size - m_offset) : 0; }
    bool hasMore() const { return m_offset < m_size; }

    bool readArrayHeader(size_t& outSize) {
        if (!hasMore()) return false;
        uint8_t b = m_data[m_offset++];
        if ((b & 0xf0) == 0x90) {
            outSize = b & 0x0f;
            return true;
        } else if (b == 0xdc) {
            if (getRemaining() < 2) return false;
            outSize = (m_data[m_offset] << 8) | m_data[m_offset + 1];
            m_offset += 2;
            return true;
        } else if (b == 0xdd) {
            if (getRemaining() < 4) return false;
            outSize = ((uint32_t)m_data[m_offset] << 24) |
                      ((uint32_t)m_data[m_offset + 1] << 16) |
                      ((uint32_t)m_data[m_offset + 2] << 8) |
                      (uint32_t)m_data[m_offset + 3];
            m_offset += 4;
            return true;
        }
        m_offset--;
        return false;
    }

    bool readMapHeader(size_t& outSize) {
        if (!hasMore()) return false;
        uint8_t b = m_data[m_offset++];
        if ((b & 0xf0) == 0x80) {
            outSize = b & 0x0f;
            return true;
        } else if (b == 0xde) {
            if (getRemaining() < 2) return false;
            outSize = (m_data[m_offset] << 8) | m_data[m_offset + 1];
            m_offset += 2;
            return true;
        } else if (b == 0xdf) {
            if (getRemaining() < 4) return false;
            outSize = ((uint32_t)m_data[m_offset] << 24) |
                      ((uint32_t)m_data[m_offset + 1] << 16) |
                      ((uint32_t)m_data[m_offset + 2] << 8) |
                      (uint32_t)m_data[m_offset + 3];
            m_offset += 4;
            return true;
        }
        m_offset--;
        return false;
    }

    bool readString(std::string& outStr) {
        if (!hasMore()) return false;
        uint8_t b = m_data[m_offset++];
        size_t len = 0;

        if ((b & 0xe0) == 0xa0) {
            len = b & 0x1f;
        } else if (b == 0xd9) {
            if (getRemaining() < 1) { m_offset--; return false; }
            len = m_data[m_offset++];
        } else if (b == 0xda) {
            if (getRemaining() < 2) { m_offset--; return false; }
            len = (m_data[m_offset] << 8) | m_data[m_offset + 1];
            m_offset += 2;
        } else if (b == 0xdb) {
            if (getRemaining() < 4) { m_offset--; return false; }
            len = ((uint32_t)m_data[m_offset] << 24) |
                  ((uint32_t)m_data[m_offset + 1] << 16) |
                  ((uint32_t)m_data[m_offset + 2] << 8) |
                  (uint32_t)m_data[m_offset + 3];
            m_offset += 4;
        } else {
            m_offset--;
            return false;
        }

        if (getRemaining() < len) {
            return false;
        }

        outStr.assign(reinterpret_cast<const char*>(m_data + m_offset), len);
        m_offset += len;
        return true;
    }

    template<typename T>
    bool readInt(T& outVal) {
        if (!hasMore()) return false;
        uint8_t b = m_data[m_offset++];

        // positive fixint
        if ((b & 0x80) == 0x00) {
            outVal = static_cast<T>(b);
            return true;
        }
        // negative fixint
        if ((b & 0xe0) == 0xe0) {
            outVal = static_cast<T>((int8_t)b);
            return true;
        }
        if (b == 0xcc) { // uint8
            if (getRemaining() < 1) { m_offset--; return false; }
            outVal = static_cast<T>(m_data[m_offset++]);
            return true;
        }
        if (b == 0xd0) { // int8
            if (getRemaining() < 1) { m_offset--; return false; }
            outVal = static_cast<T>((int8_t)m_data[m_offset++]);
            return true;
        }
        if (b == 0xcd) { // uint16
            if (getRemaining() < 2) { m_offset--; return false; }
            outVal = static_cast<T>((m_data[m_offset] << 8) | m_data[m_offset + 1]);
            m_offset += 2;
            return true;
        }
        if (b == 0xd1) { // int16
            if (getRemaining() < 2) { m_offset--; return false; }
            outVal = static_cast<T>((int16_t)((m_data[m_offset] << 8) | m_data[m_offset + 1]));
            m_offset += 2;
            return true;
        }
        if (b == 0xce || b == 0xd2) { // uint32 / int32
            if (getRemaining() < 4) { m_offset--; return false; }
            outVal = static_cast<T>(((uint32_t)m_data[m_offset] << 24) |
                                    ((uint32_t)m_data[m_offset + 1] << 16) |
                                    ((uint32_t)m_data[m_offset + 2] << 8) |
                                    (uint32_t)m_data[m_offset + 3]);
            m_offset += 4;
            return true;
        }

        m_offset--;
        return false;
    }

    bool readBool(bool& outVal) {
        if (!hasMore()) return false;
        uint8_t b = m_data[m_offset++];
        if (b == 0xc3) {
            outVal = true;
            return true;
        } else if (b == 0xc2) {
            outVal = false;
            return true;
        }
        m_offset--;
        return false;
    }

    void skipValue() {
        if (!hasMore()) return;
        uint8_t b = m_data[m_offset++];

        // fixint / nil / bool
        if ((b & 0x80) == 0x00 || (b & 0xe0) == 0xe0 || b == 0xc0 || b == 0xc2 || b == 0xc3) {
            return;
        }
        // fixstr
        if ((b & 0xe0) == 0xa0) {
            size_t len = b & 0x1f;
            m_offset += (getRemaining() >= len) ? len : getRemaining();
            return;
        }
        // fixarray
        if ((b & 0xf0) == 0x90) {
            size_t count = b & 0x0f;
            for (size_t i = 0; i < count; i++) skipValue();
            return;
        }
        // fixmap
        if ((b & 0xf0) == 0x80) {
            size_t count = b & 0x0f;
            for (size_t i = 0; i < count * 2; i++) skipValue();
            return;
        }
        if (b == 0xcc || b == 0xd0) { m_offset += 1; return; }
        if (b == 0xcd || b == 0xd1) { m_offset += 2; return; }
        if (b == 0xce || b == 0xd2 || b == 0xca) { m_offset += 4; return; }
        if (b == 0xcf || b == 0xd3 || b == 0xcb) { m_offset += 8; return; }

        if (b == 0xd9) { // str 8
            if (getRemaining() >= 1) {
                uint8_t len = m_data[m_offset++];
                m_offset += (getRemaining() >= len) ? len : getRemaining();
            }
            return;
        }
        if (b == 0xda) { // str 16
            if (getRemaining() >= 2) {
                uint16_t len = (m_data[m_offset] << 8) | m_data[m_offset + 1];
                m_offset += 2;
                m_offset += (getRemaining() >= len) ? len : getRemaining();
            }
            return;
        }
        if (b == 0xdc) { // array 16
            if (getRemaining() >= 2) {
                uint16_t count = (m_data[m_offset] << 8) | m_data[m_offset + 1];
                m_offset += 2;
                for (size_t i = 0; i < count; i++) skipValue();
            }
            return;
        }
        if (b == 0xde) { // map 16
            if (getRemaining() >= 2) {
                uint16_t count = (m_data[m_offset] << 8) | m_data[m_offset + 1];
                m_offset += 2;
                for (size_t i = 0; i < count * 2; i++) skipValue();
            }
            return;
        }
    }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_offset;
};

} // namespace msgpack
} // namespace cbdos
