#pragma once

#include <stddef.h>
#include <stdint.h>

namespace max31865_cli
{
/** Fixed-storage serial line editor; consumes at most 32 bytes per poll. */
class BoundedSerialShell
{
public:
    enum class Result : uint8_t
    {
        None = 0,
        Line,
        Overflow
    };

    template <typename StreamType>
    Result poll(StreamType &stream, char *out, size_t capacity)
    {
        if (out == nullptr || capacity < 2U)
        {
            return Result::Overflow;
        }
        size_t consumed = 0U;
        while (consumed < MAX_BYTES_PER_POLL && stream.available() > 0)
        {
            const int received = stream.read();
            if (received < 0)
            {
                break;
            }
            ++consumed;
            const char value = static_cast<char>(received);
            if (value == '\b' || value == 0x7F)
            {
                if (!_overflow && _length > 0U)
                {
                    --_length;
                }
                continue;
            }
            if (value == '\r' || value == '\n')
            {
                if (_overflow)
                {
                    reset();
                    return Result::Overflow;
                }
                if (_length == 0U)
                {
                    continue;
                }
                if (_length >= capacity)
                {
                    reset();
                    return Result::Overflow;
                }
                for (size_t index = 0U; index < _length; ++index)
                {
                    out[index] = _input[index];
                }
                out[_length] = '\0';
                reset();
                return Result::Line;
            }
            if (_overflow)
            {
                continue;
            }
            if (_length >= sizeof(_input) - 1U)
            {
                _overflow = true;
                _length = 0U;
                continue;
            }
            _input[_length++] = value;
        }
        return Result::None;
    }

private:
    static constexpr size_t MAX_BYTES_PER_POLL = 32U;
    char _input[192]{};
    size_t _length = 0U;
    bool _overflow = false;

    void reset()
    {
        _length = 0U;
        _overflow = false;
    }
};
} // namespace max31865_cli
