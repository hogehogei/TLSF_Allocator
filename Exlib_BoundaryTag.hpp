
#ifndef   EXLIB_BOUDNARY_TAG_HPP
#define   EXLIB_BOUDNARY_TAG_HPP


#include <stdint.h>
#include <cassert>

// メモリ破壊を検出するためのコードを有効
//#define   ENABLE_CHECK_MEMORY_BREAK

namespace exlib
{

//class BoundaryTagEnd;
/**
 * @brief  TLSFアロケータ用  メモリブロックエンドヘッダ
 **/
class BoundaryTagEnd 
{
public:
    /**
     * @param[in]  size  管理するメモリブロックサイズ
     **/
    BoundaryTagEnd( uint32_t size );
    ~BoundaryTagEnd();
    
    //!  管理するメモリブロックサイズ
    void setSize( uint32_t size );

    /**
     * @retval  メモリブロックサイズ
     **/
    uint32_t size() const { return mMemSize; }
    
private:
    
    uint32_t mMemSize;   //! メモリサイズ
#ifdef   ENABLE_CHECK_MEMORY_BREAK
    static const int skMemCheckVal = 0xA3B55B3A;
    uint32_t mMemCheck;  //! メモリ破壊検出用の値をいれる
#endif

};

/**
 * @brief  TLSFアロケータ用  メモリブロックヘッダ
 **/
class BoundaryTagBegin
{
public:
    
    BoundaryTagBegin();
    BoundaryTagBegin( uint32_t size );
    ~BoundaryTagBegin();
    
    void setUsed( bool is_used )
    {
        mHeader.is_used = is_used ? 1 : 0;
    }
    void setSize( uint32_t size )
    {
        mHeader.size = size;
    }

    bool merge( BoundaryTagBegin* next );
    BoundaryTagBegin* split( uint32_t size );
    
    BoundaryTagBegin* prevTag()
    {
        uint8_t* p = reinterpret_cast<uint8_t*>( this );
        BoundaryTagEnd* prev_end = reinterpret_cast<BoundaryTagEnd*>(p - sizeof(BoundaryTagEnd));
        p -= skHeaderOverhead + prev_end->size() + sizeof(BoundaryTagEnd);
        return reinterpret_cast<BoundaryTagBegin*>( p );

    }
    BoundaryTagBegin* nextTag()
    {
        uint8_t* p = reinterpret_cast<uint8_t*>( this );
        p += skHeaderOverhead + this->size() + sizeof(BoundaryTagEnd);
        return reinterpret_cast<BoundaryTagBegin*>( p );
    }

    BoundaryTagEnd* endTag();
    
    void detach();
    void setNextLink( BoundaryTagBegin* p ) { mNextLink = p; }
    void setPrevLink( BoundaryTagBegin* p ) { mPrevLink = p; }
    BoundaryTagBegin* nextLink() { return mNextLink; }
    BoundaryTagBegin* prevLink() { return mPrevLink; }
    
    bool isUsed() const { return mHeader.is_used; }
    uint32_t size() const { return mHeader.size; }
    
    struct Header {
        Header() : size(0), is_used(0) {}

        uint32_t size;     //! 管理するメモリブロックサイズ
        uint32_t is_used;  //! メモリブロック使用中？
    };

    static const int skHeaderOverhead = sizeof(BoundaryTagBegin::Header);
    static const int skBlockOverhead = skHeaderOverhead + sizeof(BoundaryTagEnd);

private:
    
    BoundaryTagBegin::Header mHeader;
    BoundaryTagBegin* mNextLink;
    BoundaryTagBegin* mPrevLink;
};


BoundaryTagBegin* newTag( uint32_t size, uint8_t* p );
void deleteTag( BoundaryTagBegin* b );

}

#endif   // EXLIB_BOUDNARY_TAG_HPP



