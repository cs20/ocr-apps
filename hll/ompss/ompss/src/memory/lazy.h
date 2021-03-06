
#ifndef LAZY_H
#define LAZY_H

#include <memory>
#include <type_traits>

#include <ocr.h>

namespace ocr {

template < typename T >
struct IsGuidBased : public std::is_same<ocrGuid_t,typename T::handle_type> {};

} // namespace ocr

namespace mem {

template<
    typename T,
    bool GuidBased = ocr::IsGuidBased<T>::value
>
class Lazy;

// Generic lazy initialization
// Relies on a boolean to record initialization status
// so it is not very efficient
template< typename T >
class Lazy<T,false> {
    typedef typename std::aligned_storage<sizeof(T),alignof(T)>::type Buffer;
public:
    Lazy() :
        _initialized(false)
    {
    }

    ~Lazy() {
        if( initialized() )
            value()->~T();
    }

    T& operator=( const T& o ) {
        if( initialized() ) {
            (**this) = o;
        } else {
            new(&_buffer) T(o);
            _initialized = true;
        }
        return *this;
    }

    T& operator=( T&& o ) {
        if( initialized() ) {
            (**this) = std::forward(o);
        } else {
            new(&_buffer) T(std::forward(o));
            _initialized = true;
        }
        return *this;
    }

    template < typename... Args >
    void initialize( Args&&... args ) {
        dbg_check( !initialized() );
        new(&_buffer) T( std::forward(args)... );
        _initialized = true;
    }

    template < typename... Args >
    void reset( Args&&... args ) {
        erase();
        initialize( std::forward(args)... );
    }

    // Treat managed object as if it wasn't
    // initialized
    void release() {
        _initialized = false;
    }

    void erase() {
        if( initialized() ) {
            value()->~T();
            _initialized = false;
        }
    }

    T& operator*() {
        return *value();
    }

    const T& operator*() const {
        return *value();
    }

    T* operator->() {
        return value();
    }

    const T* operator->() const {
        return value();
    }

    bool initialized() const {
        return _initialized;
    }

private:
    T* value() {
        return static_cast<T*>(
            static_cast<void*>(&_buffer)
        );
    }

    const T* value() const {
        return static_cast<const T*>(
            static_cast<void*>(&_buffer)
        );
    }

    Buffer _buffer;
    bool   _initialized;
};

// Lazy initialization for GUID based objects
// It relies on NULL_GUID special value to identify initialized
// objects, so it does not require use of booleans
// T must implement an explicit cast operator to ocrGuid_t
template< typename T >
class Lazy<T,true> {
    typedef typename std::aligned_storage<sizeof(T),alignof(T)>::type Buffer;
public:
    Lazy()
    {
        release();
    }

    ~Lazy() {
        if( initialized() )
            value()->~T();
    }

    T& operator=( const T& o ) {
        if( initialized() ) {
            (**this) = o;
        } else {
            new(&_buffer) T(o);
        }
        return *this;
    }

    T& operator=( T&& o ) {
        if( initialized() ) {
            (**this) = std::forward<T>(o);
        } else {
            new(&_buffer) T(std::forward<T>(o));
        }
        return *this;
    }

    template < typename... Args >
    void initialize( Args&&... args ) {
        dbg_check( !initialized() );
        new(&_buffer) T( std::forward(args)... );
    }

    template < typename... Args >
    void reset( Args&&... args ) {
        erase();
        initialize( std::forward(args)... );
    }

    // Treat managed object as if it wasn't
    // initialized
    void release() {
        value()->handle() = NULL_GUID;
    }

    void erase() {
        if( initialized() ) {
            value()->~T();
            release();
        }
    }

    T& operator*() {
        return *value();
    }

    const T& operator*() const {
        return *value();
    }

    T* operator->() {
        return value();
    }

    const T* operator->() const {
        return value();
    }

    bool initialized() const {
        return !ocrGuidIsNull( value()->handle() );
    }

private:
    T* value() {
        return reinterpret_cast<T*>(&_buffer);
    }

    const T* value() const {
        return reinterpret_cast<const T*>(&_buffer);
    }

    Buffer    _buffer;
};

} // namespace mem

#endif

