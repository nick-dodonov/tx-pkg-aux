#pragma once
#include <string>

namespace String
{
    /// A helper class to provide a C-style null-terminated string from a
    /// std::string_view, using stack allocation for small strings and heap
    /// allocation for larger strings.
    //// Usage:
    /// ```
    /// std::string_view sv = ...;
    /// String::CstrView cstrView(sv);
    /// const char* cstr = cstrView.c_str();
    /// ```
    template <size_t MaxStackSize = 64>
    class CstrView
    {
    public:
        explicit CstrView(std::string_view sv)
            : sv_(sv)
        {
            if (!sv.empty() && sv[sv.size()] == '\0') {
                ptr_ = sv.data();
                return;
            }

            // Use stack buffer if it fits
            if (sv.size() < MaxStackSize) {
                std::memcpy(stack_buffer_.data(), sv.data(), sv.size());
                stack_buffer_[sv.size()] = '\0';
                ptr_ = stack_buffer_.data();
                return;
            }

            // Fallback: heap allocation
            heap_ = sv;
            ptr_ = heap_.c_str();
        }

        ~CstrView() = default;

        CstrView(const CstrView&) = delete;
        CstrView& operator=(const CstrView&) = delete;

        CstrView(CstrView&&) = default;
        CstrView& operator=(CstrView&&) = default;

        [[nodiscard]] const char* c_str() const noexcept { return ptr_; }
        operator const char*() const noexcept { return ptr_; }

    private:
        std::string_view sv_;
        const char* ptr_;
        std::array<char, MaxStackSize> stack_buffer_;
        std::string heap_;
    };
}