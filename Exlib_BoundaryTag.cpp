
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

#if 0
void BoundaryTagBegin::setUsed( bool is_used )
{
    uint32_t flag = 0x80000000;
    mBeginTag = is_used ? mBeginTag | 0x80000000 : mBeginTag & ~0x80000000;
    /*
    if( is_used ){
        mBeginTag |= flag;
    }
    else {
        mBeginTag &= ~flag;
    }
    */
}
#endif

#if 0
void BoundaryTagBegin::setSize( uint32_t size )
{
    assert( size <= 0x7FFFFFFF && "Over maximum memory size!" ); 
    mBeginTag = (mBeginTag & 0x80000000) | (0x7FFFFFFF & size);
}
#endif

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
#if 0
BoundaryTagBegin* BoundaryTagBegin::prevTag()
{
    uint8_t* p = reinterpret_cast<uint8_t*>( this );
    BoundaryTagEnd* prev_end = reinterpret_cast<BoundaryTagEnd*>( p - sizeof(BoundaryTagEnd) );
    
    uint32_t size = prev_end->size();
    return reinterpret_cast<BoundaryTagBegin*>( p - size );
}

BoundaryTagBegin* BoundaryTagBegin::nextTag()
{
    uint8_t* p = reinterpret_cast<uint8_t*>( this );
    p += sizeof(BoundaryTagBegin) + size() + sizeof(BoundaryTagEnd);
    return reinterpret_cast<BoundaryTagBegin*>( p );
}
#endif

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
#if 0
void BoundaryTagBegin::setNextLink( BoundaryTagBegin* p )
{
    mNextLink = p;
}

void BoundaryTagBegin::setPrevLink( BoundaryTagBegin* p )
{
    mPrevLink = p;
}

/*
BoundaryTagBegin* BoundaryTagBegin::nextLink()
{
    return mNextLink;
}
*/
/*
BoundaryTagBegin* BoundaryTagBegin::prevLink()
{
    return mPrevLink;
}
*/

bool BoundaryTagBegin::isUsed() const
{
    return (mBeginTag & 0x80000000);
//    return mBeginTag >> 31;
}
#if 0
uint32_t BoundaryTagBegin::size() const
{
    return (mBeginTag & 0x7FFFFFFF);
}
#endif
#endif


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
    //size += sizeof(BoundaryTagBegin) + sizeof(BoundaryTagEnd);
    mMemSize = size;
}

/*
bool BoundaryTagEnd::isUsed() const
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>( this );
    p -= this->size() + BoundaryTagBegin::skHeaderOverhead;
    
    return reinterpret_cast<const BoundaryTagBegin*>( p )->isUsed();
}
*/




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

