
#include "Exlib_TLSFAllocator.hpp"
#include "Exlib_TLSFAllocatorImpl.hpp"

namespace exlib
{

TLSF_Allocator::TLSF_Allocator( uint32_t size )
    : mImpl( std::make_shared<TLSF_AllocatorImpl>( size ) )
{}

TLSF_Allocator::~TLSF_Allocator() throw()
{}

void* TLSF_Allocator::allocate( uint32_t size )
{
    return mImpl->allocate( size );
}

void TLSF_Allocator::deallocate( void* ptr )
{
    mImpl->deallocate( ptr );
}

void TLSF_Allocator::printMemoryList()
{
    mImpl->printMemoryList();
}

}

