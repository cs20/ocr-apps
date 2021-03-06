
#ifndef TRACEBLOCK_H
#define TRACEBLOCK_H

#include "debug/output.h"

namespace debug {

template <
    typename N,
    typename F
>
struct TraceBlock {
    TraceBlock( N&& name, F&& file, int line ) :
        _name(name), _file(file), _line(line)
    {
        log::verbose<log::Module::any>( "Starting block; func: ", _name, " @", _file, ":", _line );
    }

    ~TraceBlock() {
        log::verbose<log::Module::any>( "End of block; func: ", _name, " @", _file, ":", _line );
    }

    N _name;
    F _file;
    int _line;
};

template < typename N, typename F >
inline TraceBlock<N,F> trace_block( N&& name, F&& file, int line )
{
    return TraceBlock<N,F>(std::forward<N>(name),std::forward<F>(file),line);
}

} // namespace debug

#ifndef RELEASE
#define PROFILE_BLOCK( block_name ) \
    auto _b_##__LINE__ = ::debug::trace_block(__func__,__FILE__,__LINE__);
#endif

#endif // TRACEBLOCK_H

