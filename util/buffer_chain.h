#pragma once

#include <cstdint>
#include <iterator>

namespace recorder
{

template <typename T>
class BufferChain
{
public:
    struct Link
    {
        T* buffer;
        uint32_t length;

        uint32_t size(void) const
        {
            return length * sizeof(T);
        }

        uint32_t offset;
    };

    template <uint32_t num_links>
    void Init(Link (&chain)[num_links])
    {
        num_links_ = num_links;
        chain_ = chain;
        total_size_ = 0;

        for (auto& link : chain)
        {
            link.offset = total_size_;
            total_size_ += link.size();
        }
    }

    T& operator[](size_t index)
    {
        for (uint32_t i = 0; i < num_links_; i++)
        {
            if (index < chain_[i].length)
            {
                return chain_[i].buffer[index];
            }

            index -= chain_[i].length;
        }

        return dummy_;
    }

    uint32_t size(void)
    {
        return total_size_;
    }

    uint32_t length(void)
    {
        return total_size_ / sizeof(T);
    }

    class iter
    {
    public:
        iter() {}
        iter(const Link* chain, uint32_t num) : chain_(chain), num_(num) {}
        iter& operator++() {num_++; return *this;}
        iter operator++(int) {iter ret = *this; ++(*this); return ret;}
        bool operator==(iter other) const {return num_ == other.num_;}
        bool operator!=(iter other) const {return !(*this == other);}
        const Link& operator*() {return chain_[num_];}

    protected:
        const Link* chain_;
        uint32_t num_ = 0;
    };

    iter begin() {return iter(chain_, 0);}
    iter end() {return iter(chain_, num_links_);}

protected:
    uint32_t num_links_;
    Link* chain_;
    uint32_t total_size_;
    T dummy_;
};

}
