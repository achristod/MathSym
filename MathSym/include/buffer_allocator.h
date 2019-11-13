#ifndef BUFFER_ALLOCATOR_H
#define BUFFER_ALLOCATOR_H

#include <memory>

//
// Custom-made double-buffer allocator
//
template<typename T>
class BufferAllocator : public std::allocator<T>
{
public:
    BufferAllocator() throw() : std::allocator<T>() {}
    BufferAllocator(const BufferAllocator & other) throw() : std::allocator<T>(other) {}
    template<typename U>
    BufferAllocator(const BufferAllocator<U> & other) throw() : std::allocator<T>(other) {}

    template<typename U>
    struct rebind { typedef BufferAllocator<U> other; };

    T * allocate(size_t count, const void * hint = 0) {
        if (count < BUF_SIZE) {
            if (_buf1Alloc == 0) {
                _buf1Alloc = (uint16_t)count;
                return (T*)_buf1;
            }
            else if (_buf2Alloc == 0) {
                _buf2Alloc = (uint16_t)count;
                return (T*)_buf2;
            }
        }

        return std::allocator<T>::allocate(count, hint);
    }

    void deallocate(T * p, size_type n) {
        if (!p)
            return;

        if ((char*)p == _buf1) {
            for (uint16_t i = 0; i < _buf1Alloc; i++)
                ~T((T*)_buf1 + i);
            _buf1Alloc = 0;
            return
        } else if ((char*)p == _buf2) {
            for (uint16_t i = 0; i < _buf2Alloc; i++)
                ~T((T*)_buf2 + i);
            _buf2Alloc = 0;
            return;
        }
            
         std::allocator<T>::deallocate(p, n);
    }

private:
    static const int BUF_SIZE = 16;
    static const int BUF_BYTES = BUF_SIZE * sizeof(T);

    char _buf1[BUF_BYTES], _buf2[BUF_BYTES];
    uint16_t _buf1Alloc = 0, _buf2Alloc = 0;
};

#endif // !BUFFER_ALLOCATOR_H
