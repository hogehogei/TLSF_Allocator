
#ifndef   EXLIB_TLSF_ALLOCATOR_HPP
#define   EXLIB_TLSF_ALLOCATOR_HPP

#include <stdint.h>
#include <memory>

namespace exlib
{

class TLSF_AllocatorImpl;
class TLSF_Allocator
{
public:
    
    TLSF_Allocator( uint32_t size );
    ~TLSF_Allocator() throw();
    
    void* allocate( uint32_t size );
    void deallocate( void* ptr );
    void printMemoryList();
    
private:
    
    std::shared_ptr<TLSF_AllocatorImpl> mImpl;
};

}

#endif   // EXLIB_TLSF_ALLOCATOR_HPP


