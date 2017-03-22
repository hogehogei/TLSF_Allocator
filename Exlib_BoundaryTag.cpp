
#include "Exlib_BoundaryTag.hpp"
#include <cassert>
#include <new>

namespace exlib
{

BoundaryTagBegin::BoundaryTagBegin()
    : mNextLink( 0 ),
      mPrevLink( 0 ),
      mHeader()
{
    setSize( 0 );
}

BoundaryTagBegin::BoundaryTagBegin( uint32_t size )
    : mNextLink( 0 ),
      mPrevLink( 0 ),
      mHeader()
{
    setSize( size );
}

BoundaryTagBegin::~BoundaryTagBegin()
{
}

bool BoundaryTagBegin::merge( BoundaryTagBegin* next )
{
    if( this->isUsed() || next->isUsed() ){
        return false;
    } 
    BoundaryTagEnd* end = this->endTag();
    
    BoundaryTagEnd* new_end = next->endTag();
    uint32_t new_bufsize = this->size() + BoundaryTagBegin::skBlockOverhead + next->size();
    this->setSize( new_bufsize );
    new_end->setSize( new_bufsize );
    
    // calling destructor explicitly 
    end->~BoundaryTagEnd();
    next->~BoundaryTagBegin();
    
    return true;
}

BoundaryTagBegin* BoundaryTagBegin::split( uint32_t size )
{
    assert( this->size() > size && "Tag size is larger than the original tag. Out of Memory!" );
    
    uint32_t left_size = size;
    uint32_t right_size = this->size() - size;
    
    if( right_size <= sizeof(BoundaryTagBegin) + sizeof(BoundaryTagEnd) ){
        // ‚à‚¤•Ð•û‚Ìƒ^ƒO‚Ì—Ìˆæ‚ª‚È‚­‚Ä•ªŠ„‚Å‚«‚È‚¢
        return 0;
    }
    right_size -= BoundaryTagBegin::skHeaderOverhead + sizeof(BoundaryTagEnd);
    
    this->setSize( left_size );
    
    uint8_t* p = reinterpret_cast<uint8_t*>( this );
    p += BoundaryTagBegin::skHeaderOverhead + this->size();
    BoundaryTagEnd* left_end = new(p) BoundaryTagEnd( left_size );
    
    p += sizeof(BoundaryTagEnd);
    BoundaryTagBegin* right_begin = new(p) BoundaryTagBegin( right_size );
    
    p += BoundaryTagBegin::skHeaderOverhead + right_begin->size();
    BoundaryTagEnd* right_end = new(p) BoundaryTagEnd( right_size );
    
    return right_begin;
}

BoundaryTagEnd* BoundaryTagBegin::endTag() 
{
    uint8_t* p = reinterpret_cast<uint8_t*>( this );
    p += BoundaryTagBegin::skHeaderOverhead + this->size();
    
    return reinterpret_cast<BoundaryTagEnd*>( p );
}

void BoundaryTagBegin::detach()
{
    if( mPrevLink ){
        mPrevLink->setNextLink( mNextLink );
    }
    if( mNextLink ){
        mNextLink->setPrevLink( mPrevLink );
    }
    mPrevLink = nullptr;   
    mNextLink = nullptr;
}

BoundaryTagEnd::BoundaryTagEnd( uint32_t size )
{
    setSize( size );
#ifdef   ENABLE_CHECK_MEMORY_BREAK
    mMemCheck = skMemCheckVal;
#endif
}

BoundaryTagEnd::~BoundaryTagEnd()
{
#ifdef   ENABLE_CHECK_MEMORY_BREAK
    assert( mMemCheck == skMemCheckVal && "Memory Corrupt! IT MUST BE BUG" );
#endif
}

void BoundaryTagEnd::setSize( uint32_t size )
{
    mMemSize = size;
}

BoundaryTagBegin* newTag( uint32_t size, uint8_t* p )
{
    exlib::BoundaryTagBegin* b = new(p) exlib::BoundaryTagBegin( size );
    p += BoundaryTagBegin::skHeaderOverhead + b->size();
    exlib::BoundaryTagEnd* e = new(p) exlib::BoundaryTagEnd( size );
    
    return b;
}

void deleteTag( BoundaryTagBegin* b )
{
    BoundaryTagEnd* e = b->endTag();
    
    b->~BoundaryTagBegin();
    e->~BoundaryTagEnd();
}

}

