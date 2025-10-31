#pragma once

#include <supp/IntrusiveForwardList.h>

namespace exec {

struct FrameHeader : supp::IntrusiveForwardListNode<> {
    size_t size = 0;
};

class FrameAllocator {
 public:
    FrameAllocator() = default;

    void* allocate(size_t size) noexcept {

    }

    void deallocate(void* ptr, size_t size) {

    }

 private:
    supp::IntrusiveForwardList<FrameHeader> allocated_;
};

}  // namespace exec
