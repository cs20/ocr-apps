#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "libswtest.h"
//
// These are defined in xe-sim/include/xe-abi.h
//
#define XE_ASSERT_ERROR 0xC1    // application failure indication
#define XE_READY        0xF0    // provided by crt0 to indicate 'alive-ness'
#define XE_MSG_READY    0xF1    // service request
#define XE_CONOUT       0xFE    // simple string out to fsim 'console'
#define XE_TERMINATE    0xFF    // provided by exit() to indicate done

//
// XE MSRs, defined in intel/tg/common/include/mmio-table.def
//
#define CORE_LOCATION_NUM   12

XE_Id get_xe_id( void )
{
    XE_Id core_loc;

    __asm__ __volatile__(
        "loadmsr %0, %1, 64"
        : /* outs */ "={r1}" (core_loc)
        : /* ins  */  "L" (CORE_LOCATION_NUM)
        );
    return core_loc;
}

//
// Cache builtin wrapper
// Flush (Write-back) and/or invalidate lines in the addr range
//
void cache_range( unsigned char flags, void * from, void * to )
{
    switch( flags ) {
        case CACHE_WBINVAL:
        __builtin_xstg_cache_range( CACHE_WBINVAL, from, to ); break;
        // Disable until assembly output issue fixed (I/D instead of 1/0)
        //case CACHE_INVAL:
        //  __builtin_xstg_cache_range( CACHE_INVAL, from, to ); break;
        case CACHE_WB:
        __builtin_xstg_cache_range( CACHE_WB, from, to ); break;
        default:
        break;
    }
}

void ce_exit( int retval )
{
    __asm__ __volatile__(
        "alarm %2"
        : /* no outputs */ : "{r2}" (retval), "{r3}" (0), "L" (XE_TERMINATE)
        );
    __builtin_unreachable();
}

void ce_abort( )
{
    __asm__ __volatile__(
        "alarm %2"
        : /* no outputs */ : "{r2}" (0), "{r3}" (0), "L" (XE_ASSERT_ERROR)
        );
    __builtin_unreachable();
}

void ce_print( char *msg )
{
    char * p = msg;
    while( *p++ )    // will count the end '\0'
    ;
    __asm__ __volatile__(
        "alarm %2"
        : /* no outputs */ : "{r2}" (msg), "{r3}" (p - msg), "L" (XE_CONOUT)
        );
}

//
// from fsim ce-handle-alarm.c
//
typedef enum {
    CE_REQTYPE_FIRST    = 0,
    CE_REQTYPE_MEMALLOC = CE_REQTYPE_FIRST,
    CE_REQTYPE_MEMFREE,
    CE_REQTYPE_GETCWD,
    CE_REQTYPE_CHDIR,
    CE_REQTYPE_CHMOD,
    CE_REQTYPE_CHOWN,
    CE_REQTYPE_LINK,
    CE_REQTYPE_SYMLINK,
    CE_REQTYPE_UNLINK,
    CE_REQTYPE_FILEOPEN,
    CE_REQTYPE_FILECLOSE,
    CE_REQTYPE_FILEREAD,
    CE_REQTYPE_FILEWRITE,
    CE_REQTYPE_FILELSEEK,
    CE_REQTYPE_FILESTAT,
    CE_REQTYPE_FILEFSTAT,
    CE_REQTYPE_GETTIMEOFDAY,
    CE_REQTYPE_LAST
} ce_request_type;

#define CE_REQ_MAKE( t, l )    (((t) << 32) | ((l) & ((1L << 32) - 1)))
//
// common method to signal CE wrt a request
//
static int send_req( uint64_t type, void * buf, uint64_t len )
{
    uint64_t r2 = CE_REQ_MAKE( type, len );
    uint64_t status;

    cache_range( CACHE_WBINVAL, buf, buf + len );

    __asm__ __volatile__(
        "flush B, N\n\t"
        "fence 0xF, B\n\t"
        "alarm %3"
        : /* outs */ "={r2}" (status)
        : /* ins */ "{r2}" (r2), "{r3}" (buf), "L" (XE_MSG_READY)
        );
    return status;
}

void * ce_memalloc( uint64_t len )
{
    struct {
        uint64_t  region;       // in - region preference (global, block, local) (Not implemented)
        uint64_t  va;           // out
        uint64_t  len : 56,     // in/out - size requested and actually provided
                  private : 1,  // in - private use (reapable on finish) (Not implemented)
                  promote : 1,  // in - promote region if alloc fails (Not implemented)
                  unused : 6;
    } req;
    req.va = 0;
    req.len = len;

    int status = send_req( CE_REQTYPE_MEMALLOC, & req, sizeof(req) );

    if( ! status )
        return (void *) req.va;
    else
        return (void *) 0L;
}

void ce_memfree( void * va )
{
    struct {
        uint64_t   va;      // out
    } req;
    req.va = (uint64_t) va;

    int status = send_req( CE_REQTYPE_MEMFREE, & req, sizeof(req) );

    return;
}

//
// Get the CWD
//
char * ce_getcwd( char * buf, size_t size )
{
    struct {
        uint64_t buf_ptr;     // in
        uint64_t size;        // in
    } req;
    req.buf_ptr = (uint64_t)buf;
    req.size = size;

    int status = send_req( CE_REQTYPE_GETCWD, & req, sizeof(req) );

    if( status ) {
        errno = status;
        return NULL;
    }
    return buf;
}

//
// Change the CWD
//
int ce_chdir( const char * path )
{
    struct {
        uint64_t path_ptr;        // in
        uint64_t path_len;        // in (len not including terminal '\0')
    } req;

    const char *p = path;
    while( *p )
        p++;

    req.path_ptr = (uint64_t)path;
    req.path_len = (uint64_t)(p - path);

    int status = send_req( CE_REQTYPE_CHDIR, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    } else
        status = 0;

    return status;
}

//
// Change the permissions of file
//
int ce_chmod( const char * path, mode_t mode)
{
    struct {
        uint64_t path_ptr;        // in
        uint64_t path_len;        // in (len not including terminal '\0')
        uint64_t mode;            // in
    } req;

    const char *p = path;
    while( *p )
        p++;

    req.path_ptr = (uint64_t)path;
    req.path_len = (uint64_t)(p - path);
    req.mode = (uint64_t)mode;

    int status = send_req( CE_REQTYPE_CHMOD, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    } else
        status = 0;

    return status;
}

//
// Change the ownership of file
//
int ce_chown( const char * path, uid_t owner, gid_t group)
{
    struct {
        uint64_t path_ptr;        // in
        uint64_t path_len;        // in (len not including terminal '\0')
        uint64_t owner;           // in
        uint64_t group;           // in
    } req;

    const char *p = path;
    while( *p )
        p++;

    req.path_ptr = (uint64_t)path;
    req.path_len = (uint64_t)(p - path);
    req.owner = (uint64_t)owner;
    req.group = (uint64_t)group;

    int status = send_req( CE_REQTYPE_CHOWN, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    } else
        status = 0;

    return status;
}

//
// Make a new name for a file
//
int ce_link( const char * oldpath, const char * newpath)
{
    struct {
        uint64_t oldpath_ptr;        // in
        uint64_t oldpath_len;        // in (len not including terminal '\0')
        uint64_t newpath_ptr;        // in
        uint64_t newpath_len;        // in (len not including terminal '\0')
    } req;

    const char *p = oldpath;
    while( *p )
        p++;

    req.oldpath_ptr = (uint64_t)oldpath;
    req.oldpath_len = (uint64_t)(p - oldpath);

    p = newpath;
    while( *p )
        p++;

    req.newpath_ptr = (uint64_t)newpath;
    req.newpath_len = (uint64_t)(p - newpath);

    int status = send_req( CE_REQTYPE_LINK, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    } else
        status = 0;

    return status;
}

//
// Make a new symbolic link for a file
//
int ce_symlink( const char * oldpath, const char * newpath)
{
    struct {
        uint64_t oldpath_ptr;        // in
        uint64_t oldpath_len;        // in (len not including terminal '\0')
        uint64_t newpath_ptr;        // in
        uint64_t newpath_len;        // in (len not including terminal '\0')
    } req;

    const char *p = oldpath;
    while( *p )
        p++;

    req.oldpath_ptr = (uint64_t)oldpath;
    req.oldpath_len = (uint64_t)(p - oldpath);

    p = newpath;
    while( *p )
        p++;

    req.newpath_ptr = (uint64_t)newpath;
    req.newpath_len = (uint64_t)(p - newpath);

    int status = send_req( CE_REQTYPE_SYMLINK, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    } else
        status = 0;

    return status;
}

//
// Remove a name from filesystem.
//
int ce_unlink( const char * pathname )
{
    struct {
        uint64_t path_ptr;        // in
        uint64_t path_len;        // in (len not including terminal '\0')
    } req;

    const char *p = pathname;
    while( *p )
        p++;

    req.path_ptr = (uint64_t)pathname;
    req.path_len = (uint64_t)(p - pathname);

    int status = send_req( CE_REQTYPE_UNLINK, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    } else
        status = 0;

    return status;
}

//
// File open
//
int ce_open( const char * file, int flags, int mode )
{
    struct {
        uint64_t fname_ptr; // in
        uint64_t fname_len; // in (len of fname not including terminal '\0')
        uint64_t flags;     // in
        uint64_t mode;      // in
        uint64_t fd;        // out
    } req;
    req.flags = flags;
    req.mode = mode;
    req.fd = 0;

    req.fname_ptr = (uint64_t) file;

    const char *p = file;

    while( *p )
        p++;
    req.fname_len = p - file;

    int status = send_req( CE_REQTYPE_FILEOPEN, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    } else
        status = req.fd;

    return status;
}

//
// File close
//
int ce_close( int fd )
{
    struct {
        uint64_t fd; // in
    } req;
    req.fd = fd;

    int status = send_req( CE_REQTYPE_FILECLOSE, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    }
    return status;
}

//
// File read
//
int ce_read( int fd, void * buf, size_t count )
{
    struct {
        uint64_t fd;        // in
        uint64_t buf;       // in
        uint64_t len;       // in / out
    } req;
    req.fd = fd;
    req.buf = (uint64_t) buf;
    req.len = count;

    int status = send_req( CE_REQTYPE_FILEREAD, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    } else
        status = req.len;   // now bytes read

    return status;
}

//
// File write
//
int ce_write( int fd, const void * buf, size_t count )
{
    struct {
        uint64_t fd;        // in
        uint64_t buf;       // in
        uint64_t len;       // in / out
    } req;
    req.fd = fd;
    req.buf = (uint64_t) buf;
    req.len = count;

    int status = send_req( CE_REQTYPE_FILEWRITE, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    } else
        status = req.len;   // now bytes written

    return status;
}

//
// lseek
//
off_t ce_lseek( int fd, off_t offset, int whence )
{
    struct {
        uint64_t fd;     // in
        uint64_t offset; // in/out
        uint64_t whence; // in
    } req;
    req.fd = fd;
    req.offset = (uint64_t) offset;
    req.whence = whence;

    int status = send_req( CE_REQTYPE_FILELSEEK, & req, sizeof(req) );

    off_t retval = (off_t) -1;

    if( status ) {
        errno = status;
    } else
        retval = (off_t) req.offset;   // new offset

    return retval;
}


//
// File stat
//
int ce_stat( const char * file, struct stat * stbuf )
{
    struct {
        uint64_t fname_ptr; // in
        uint64_t fname_len; // in (len of fname not including terminal '\0')
        uint64_t stat_ptr;  // in
        uint64_t stat_len;  // in (paranoia)
    } req;
    req.fname_ptr = (uint64_t) file;
    const char *p = file;

    while( *p )
        p++;
    req.fname_len = p - file;

    req.stat_ptr = (uint64_t) stbuf;
    req.stat_len = sizeof(*stbuf);

    int status = send_req( CE_REQTYPE_FILESTAT, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    }
    return status;
}
//
// File fstat
//
int ce_fstat( int fd, struct stat * stbuf )
{
    struct {
        uint64_t fd;        // in
        uint64_t stat_ptr;  // in
        uint64_t stat_len;  // in (paranoia)
    } req;
    req.fd = fd;
    req.stat_ptr = (uint64_t) stbuf;
    req.stat_len = sizeof(*stbuf);

    int status = send_req( CE_REQTYPE_FILEFSTAT, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    }
    return status;
}

int ce_gettimeofday( struct timeval  *ptimeval, void *ptimezone )
{
    struct {
        uint64_t timeval_ptr; // in
        uint64_t timezone_ptr; // in
    } req;
    req.timeval_ptr = (uint64_t) ptimeval;
    req.timezone_ptr = (uint64_t) ptimezone;

    int status = send_req( CE_REQTYPE_GETTIMEOFDAY, & req, sizeof(req) );

    if( status ) {
        errno = status;
        status = -1;
    }
    return status;
}
