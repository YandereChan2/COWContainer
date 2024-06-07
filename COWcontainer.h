#pragma once

#include <memory>
template<class T, class Allocator = std::allocator<T>> class COWcontainer
{
    struct control_block
    {
        size_t counter = 0;
        char body[sizeof(T)];
    };
    using ALLOC_T = std::allocator_traits<Allocator>;
    using CONTROL_BLOCK = COWcontainer<T, Allocator>::control_block;
    using NALLOC = ALLOC_T::template rebind_alloc<CONTROL_BLOCK>;
    using NALLOC_T = std::allocator_traits< COWcontainer<T, Allocator>::NALLOC>;
    [[no_unique_address]] NALLOC na;
    [[no_unique_address]] Allocator a;
     control_block* source;
    void release()noexcept
    {
        if (source == nullptr)
        {
            return;
        }
        if (source->counter > 1)
        {
            source->counter--;
            source = nullptr;
            return;
        }
        ALLOC_T::destroy(a, (T*)(source->body));
        na.deallocate(source, 1);
        source = nullptr;
    }
    bool unique()const noexcept
    {
        if (source == nullptr)
        {
            return false;
        }
        return source->counter == 1;
    }
    struct _scope_guard
    {
        control_block*& s;
        NALLOC& na;
        bool flag = true;
        ~_scope_guard()
        {
            if (flag)
            {
                na.deallocate(s, 1);
                s = nullptr;
            }
        }
    };
public:
    template<class... Args>
        requires requires(Args&&... args) { T(std::forward<Args>(args)...); }
    COWcontainer(Args&&... args)noexcept(noexcept(T(std::forward<Args>(args)...)))
        : source(na.allocate(1))
    {
        if constexpr (!noexcept(T(std::forward<Args>(args)...)))
        {
            _scope_guard s{ source,na };
            source->counter = 1;
            ALLOC_T::construct(a, (T*)source->body, std::forward<Args>(args)...);
            s.flag = false;
        }
        else
        {
            source->counter = 1;
            ALLOC_T::construct(a, (T*)source->body, std::forward<Args>(args)...);
        }
    }
    COWcontainer(const COWcontainer<T>& another)noexcept
        :source(another.source)
    {
        source->counter++;
    }
    COWcontainer(COWcontainer<T>&& another)noexcept 
        :source(another.source)
    {
        another.source = nullptr;
    }
    COWcontainer& operator=(const COWcontainer<T>& another)noexcept
    {
        if (std::addressof(another) == this)[[unlikely]]
        {
            return *this;
        }
        release();
        if (another.source == nullptr)
        {
            return *this;
        }
        source = another.source;
        source->counter++;
        return *this;
    }
    COWcontainer& operator=(COWcontainer<T>&& another)noexcept
    {
        if (addressof(another) == this)[[unlikely]] 
        {
            return *this;
        }
        release();
        if (another.source == nullptr)
        {
            return *this;
        }
        source = another.source;
        another.source = nullptr;
        return *this;
    }
    ~COWcontainer()
    {
        release();
    }
    const T& read()const noexcept
    {
        return *(T*)(source->body);
    }
    T& write()noexcept(noexcept(ALLOC_T::construct(a, (T*)source->body, std::declval<T>())))
    {
        if (unique())
        {
            return *(T*)(source->body);
        }
        else
        {
            if constexpr (!noexcept(ALLOC_T::construct(a, (T*)source->body, std::declval<T>())))
            {
                _scope_guard s{ source,na };
                T* tmp = (T*)(source->body);
                source->counter--;
                source = na.allocate(1);
                ALLOC_T::construct(a, (T*)source->body, *tmp);
                source->counter = 1;
                s.flag = false;
                return *(T*)(source->body);
            }
            else
            {
                T* tmp = (T*)(source->body);
                source->counter--;
                source = na.allocate(1);
                ALLOC_T::construct(a, (T*)source->body, *tmp);
                source->counter = 1;
                return *(T*)(source->body);
            }
        }
    }
};
