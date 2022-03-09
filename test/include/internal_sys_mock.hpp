#pragma once

#include "internal/sys.hpp"

#include <unistd.h>

#include <cstdint>

#include <gmock/gmock.h>

namespace internal
{

class InternalSysMock : public Sys
{
  public:
    virtual ~InternalSysMock() = default;

    MOCK_METHOD(int, open, (const char*, int), (const override));
    MOCK_METHOD(int, read, (int, void*, size_t), (const override));
    MOCK_METHOD(int, close, (int), (const override));
    MOCK_METHOD(void*, mmap, (void*, std::size_t, int, int, int, off_t),
                (const override));
    MOCK_METHOD(int, munmap, (void*, std::size_t), (const override));
    MOCK_METHOD(int, ioctl, (int, unsigned long, void*), (const override));
};

} // namespace internal
