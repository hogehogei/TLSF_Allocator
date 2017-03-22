

#ifndef   EXLIB_TLSF_ALLOCATOR_IMPL_HPP
#define   EXLIB_TLSF_ALLOCATOR_IMPL_HPP

#include <cassert>
#include <iostream>
#include <new>
#include <vector>
#include "Exlib_BoundaryTag.hpp"

// デバッグ出力有効にするにはコメントアウト
//#define   ENABLE_DEBUG_PRINT

#ifdef   ENABLE_DEBUG_PRINT
  #define  DEBUG_PRINT_FUNCNAME()  { std::cout << "call : " << __func__ << std::endl; }
#else
  #define  DEBUG_PRINT_FUNCNAME() 
#endif


// gcc以外の場合などでMSB/LSBを求めるときのテーブル
#if 0
static const int table[] = {
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4,
    4, 4,
    4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5,
    5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6,
    6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6,
    6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7,
    7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7,
    7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7,
    7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7,
    7, 7, 7, 7, 7, 7, 7
};
#endif

namespace exlib
{

class TLSF_AllocatorImpl
{    
private:
//
// Debug funcitons
//
 
#ifdef   ENABLE_DEBUG_PRINT
    void debugPrintTagInfo( BoundaryTagBegin* b )
    {
        std::cout << "block ptr : " << (int*)b << std::endl;
        std::cout << "block size : " << b->size() << std::endl;

        uint32_t fli, sli;
        getIndex( b->size(), &fli, &sli );
        std::cout << "index :  fli-> " << fli << "  sli-> " << sli << std::endl;
        printBit( mFreeListBitFLI );
        printBit( mFreeListBitSLI[fli] );
    }

    #define  DEBUG_PRINT_BLOCKINFO(b)   debugPrintTagInfo((b))
#else
    #define  DEBUG_PRINT_BLOCKINFO(b)
#endif


public:

    TLSF_AllocatorImpl( size_t buffer_size )
    {
        mSLIPartitionBits = 4; // 分割数 4bit 2^4 == 16
        mSLIPartition = (1 << mSLIPartitionBits);

        // 割り当てるメモリブロックの最小サイズを求める
        // prev/next のリンクポインタなどは、Freeブロックのリストを管理するときにしか使わないので
        // 実データを置く領域を間借りして使うようにして
        // メモリのオーバヘッドを削減している
        uint32_t linkptr_overwrap = static_cast<uint32_t>( sizeof(BoundaryTagBegin) - BoundaryTagBegin::skHeaderOverhead );
        uint32_t min_user_mem = mSLIPartition;    // 16byte
        uint32_t min_alloc_size = std::max( min_user_mem, linkptr_overwrap );  // 前後リンクポインタのサイズかユーザメモリのどちらか大きい方
        mMinAllocSize = getAlignmentedSize( min_alloc_size );
        assert( buffer_size > mMinAllocSize && "buffer_size too small!" );

        // バッファサイズを超える アラインされた領域サイズに設定
        size_t buf_alloc_size = getAlignmentedSize( buffer_size );
        assert( buf_alloc_size > mMinAllocSize && "alloc_size too small!" );

        // 管理領域タグ作成
        mPoolMemSize = buf_alloc_size + BoundaryTagBegin::skBlockOverhead + sizeof(BoundaryTagBegin);
        mPoolMem = new uint8_t[mPoolMemSize];

        // +1しているのは配列が0originだから
        // 配列操作時に getMSBの値をそのまま利用するため
        mFreeBlock = std::vector<BoundaryTagBegin*>( (getMSB( mPoolMemSize )+1) * mSLIPartition, nullptr );
        mFreeListBitFLI = 0;
        mFreeListBitSLI = std::vector<uint32_t>( getMSB( mPoolMemSize )+1, 0 );
        
        // ダミーのタグを作る
        // 前後のタグを取得するときに、範囲外のメモリにアクセスしないかのチェックを簡単にするため
        uint8_t* p = mPoolMem;
        p += mPoolMemSize - sizeof(BoundaryTagBegin);
        BoundaryTagBegin* dummy_end = new(p) BoundaryTagBegin(0);
        dummy_end->setUsed( true );
        
        // 本物の未使用領域のヘッダ
        p = mPoolMem;
        BoundaryTagBegin* beg = newTag( buf_alloc_size, p );
        registerFreeBlock( beg );
        
        assert( (unsigned long)(beg + sizeof(BoundaryTagBegin)) % skAlignment == 0 && "set alignment failed." );

//#ifdef  ENABLE_DEBUG_PRINT
        std::cout << "TLSF_Allocator info" << std::endl;
        std::cout << "alloc size     : " << buf_alloc_size << std::endl;
        std::cout << "block size     : " << beg->size() << std::endl;
        std::cout << "min block size : " << mMinAllocSize << std::endl;
        std::cout << "block_beg addr : " << (unsigned long*)beg << std::endl;
        std::cout << "block_end addr  : " << (unsigned long*)dummy_end << std::endl;
        std::cout << "header overhead : " << BoundaryTagBegin::skBlockOverhead << std::endl;
        std::cout << "link pointer size : " << sizeof(BoundaryTagBegin*)*2 << std::endl;
//#endif
    }
    
    ~TLSF_AllocatorImpl() throw()
    {
        // destruct dummy headers
        uint8_t* p = mPoolMem;
        p += mPoolMemSize - sizeof(BoundaryTagBegin);
        BoundaryTagBegin* dummy_end = reinterpret_cast<BoundaryTagBegin*>( p );
        dummy_end->~BoundaryTagBegin();
        
        // check memory block list
#ifdef  ENABLE_DEBUG_PRINT
        printMemoryList();
#endif
        delete [] mPoolMem;
        
    }

    /*
     * @usage メモリ確保
     */ 
    void* allocate( uint32_t size )
    {
        DEBUG_PRINT_FUNCNAME();

        // 確保するメモリ領域は必ずアラインメントの倍数以上にする
        uint32_t alloc_size = getAlignmentedSize( size );
        
        // 空きメモリをとってくる
        BoundaryTagBegin* free_block = searchFreeBlock( alloc_size );
        if( !free_block ){
            // メモリなし
            return 0;
        }
        assert( free_block->size() >= alloc_size && "searchFreeBlockFailed!" );

        free_block->setUsed( true );

        // FreeBlockが要求メモリサイズより大きければ分割する
        if( free_block->size() >= alloc_size + mMinAllocSize + BoundaryTagBegin::skBlockOverhead ){
            BoundaryTagBegin* remainder = free_block->split( alloc_size );
            assert( remainder && "MUST BE VALID POINTER" );
            registerFreeBlock( remainder );
        }

        uint8_t* p = reinterpret_cast<uint8_t*>( free_block );
        assert( ((unsigned long)(p + BoundaryTagBegin::skHeaderOverhead) % skAlignment == 0) && "set alignment error!" );
        DEBUG_PRINT_BLOCKINFO( reinterpret_cast<BoundaryTagBegin*>(p) );
        
        return p + BoundaryTagBegin::skHeaderOverhead;
    }
    
    void deallocate( void* ptr )
    {
        DEBUG_PRINT_FUNCNAME();

        if( !ptr ) {
            return ;
        }

        uint8_t* p = reinterpret_cast<uint8_t*>( ptr );
        BoundaryTagBegin* b = reinterpret_cast<BoundaryTagBegin*>( p - BoundaryTagBegin::skHeaderOverhead );
        deallocateBlock( b );
    }
    
    void printMemoryList()
    {
        uint8_t* p = mPoolMem;
        BoundaryTagBegin* b = 0;
        int i = 0;
        uint32_t size = 0;
        
        do {
            b = reinterpret_cast<BoundaryTagBegin*>( p );
            
            // print info
            std::cout << "Header No " << i << std::endl;
            std::cout << "Header pointer : " << reinterpret_cast<uint64_t*>( b ) << std::endl;
            std::cout << "Block size     : " << b->size() << std::endl;
            if( b->isUsed() ){
                std::cout << "Block is used." << std::endl;
            } else {
                std::cout << "Block is free." << std::endl;
            }
            std::cout << std::endl;
            
            p += BoundaryTagBegin::skHeaderOverhead + b->size() + sizeof(BoundaryTagEnd);
            b = reinterpret_cast<BoundaryTagBegin*>( p );
            size = b->size();
            ++i;
        } while( size != 0 );
    }
   

    
private:

    // debug用
    void printBit( uint32_t v )
    {
        std::cout << "31 : ";
        uint32_t mask = 0x80000000;
        for( int i = 0; i < 32; ++i ){
            if( v & mask ){
                std::cout << "1";
            }
            else {
                std::cout << "0";
            }
            mask >>= 1;
        }
        std::cout << std::endl;
    }
    
    unsigned getMSB( uint32_t v )
    {
        //uint32_t a;
        //uint32_t x = v;
        //a = x <= 0xFFFF ? (x <= 0xFF ? 0 : 8) : (x <= 0xFFFFFF ? 16 : 24);
        //return table[x >> a] + a;
        
        return 31 - __builtin_clz(v);
    }
    
    unsigned getLSB( uint32_t v )
    {
        //uint32_t a;
        //uint32_t x = v & -v;
        //a = x <= 0xFFFF ? (x <= 0xFF ? 0 : 8) : (x <= 0xFFFFFF ? 16 : 24);
        //return table[x >> a] + a;
        
        return __builtin_ctz(v);
    }
    
    unsigned calcFreeBlockIndex( unsigned FLI, unsigned SLI )
    {
        return (FLI << mSLIPartitionBits) + SLI;
    }
    
    unsigned getSecondLevelIndex( uint32_t size, unsigned msb )
    {
        int rs = msb - mSLIPartitionBits;
        
//      これと同様
//      const unsigned mask = (1 << msb) - 1;
//      return (size & mask) >> rs;
        return (size >> rs) & (mSLIPartition - 1);
    }
    
    bool getMinFreeListBit( unsigned bit, unsigned freelist_bit, unsigned* result )
    {
        const unsigned mask = 0xFFFFFFFF << bit;
        unsigned enable_list_bit = freelist_bit & mask;
        if( enable_list_bit == 0 ){
            return false;
        }
        
        *result = getLSB( enable_list_bit );
        return true;
    }
    
    /**
     * @usage 設定したアラインメントの倍数を返す
     *
     **/
    uint32_t getAlignmentedSize( uint32_t size )
    {
        if( size < mMinAllocSize ){
            return mMinAllocSize;
        }

        size = (size + (skAlignment - 1)) & ~(skAlignment - 1);
        assert( (size % skAlignment) == 0 && "set alignment failed!" );
        
        return size;
    }

    void getIndex( unsigned v, unsigned* fli, unsigned* sli )
    {
        *fli = getMSB( v );
        *sli = getSecondLevelIndex( v, *fli );
    }

    void clearBit( unsigned index, unsigned fli, unsigned sli )
    {
        assert( index == calcFreeBlockIndex( fli, sli ) && "IT MUST BE BUG" );
        
        if( !mFreeBlock[index] ){
            mFreeListBitSLI[fli] &= ~(1 << sli);
            if( !mFreeListBitSLI[fli] ){
                mFreeListBitFLI &= ~(1 << fli);
            }
        }
    }
    
    BoundaryTagBegin* searchFreeBlock( size_t alloc_size )
    {
        DEBUG_PRINT_FUNCNAME();

        unsigned fli, sli;
        getIndex( alloc_size, &fli, &sli );
        unsigned index = calcFreeBlockIndex( fli, sli );
        
        BoundaryTagBegin* b = mFreeBlock[index];
        
        if( !b ){
            if( !getMinFreeListBit( sli+1, mFreeListBitSLI[fli], &sli ) ){
                if( !getMinFreeListBit( fli+1, mFreeListBitFLI, &fli ) ){
                    return 0;   // no memory!
                }
                
                if( !getMinFreeListBit( 0, mFreeListBitSLI[fli], &sli ) ){
                    printMemoryList();
                    assert( 0 && "IT MUST BE BUG!" );
                }
            }

            index = calcFreeBlockIndex( fli, sli );
            b = mFreeBlock[index];
        }
        
        assert( b && "Invalid pointer in FreeBlockList. IT MUST BE BUG" );
        mFreeBlock[index] = b->nextLink();
        b->detach();

        // update freelist bit
        clearBit( index, fli, sli );
        DEBUG_PRINT_BLOCKINFO(b);

        return b;
    }
    
    void unregisterFreeBlock( BoundaryTagBegin* b )
    {
        DEBUG_PRINT_FUNCNAME();
        assert( b && "INVALID POINTER" );

        unsigned fli, sli, index;
        getIndex( b->size(), &fli, &sli );
        // getBlockIndex - 1 == 確実に入る大きさのfreeblock
        index = calcFreeBlockIndex( fli, sli ) - 1;

        assert( mFreeBlock[index] );
        BoundaryTagBegin* top_block = mFreeBlock[index];
        if( top_block->nextLink() ){
            if( b == top_block ){
                mFreeBlock[index] = top_block->nextLink();
            }

        }
        else {
            mFreeBlock[index] = nullptr;
            fli = index >> mSLIPartitionBits;
            sli = index & (mSLIPartition - 1);

            clearBit( index, fli, sli );
        }

        b->detach();
    }

    void registerFreeBlock( BoundaryTagBegin* b )
    {
        DEBUG_PRINT_FUNCNAME();

        BoundaryTagBegin* freeblock = b;
        uint32_t memsize = freeblock->size();
        // メモリブロックが小さすぎる。
        // Allocate時には memsize >= mMinAllocSize のはずなのでおそらくバグ
        assert( memsize >= mMinAllocSize && "memsize < mMinAllocSize  IT MUST BE BUG" );

        unsigned fli, sli, index;
        getIndex( memsize, &fli, &sli );

        index = calcFreeBlockIndex( fli, sli ) - 1;

        // フリーリストに追加
        if( mFreeBlock[index] ){
            freeblock->setNextLink( mFreeBlock[index] );
            mFreeBlock[index]->setPrevLink( freeblock );
            mFreeBlock[index] = freeblock;

            freeblock->setPrevLink( nullptr );
         }
        else {
            fli = index >> mSLIPartitionBits;
            sli = index & (mSLIPartition - 1);  // index % mMinAllocSize
            // フリーリストビットを立てておく
            mFreeListBitSLI[fli] |= (1 << sli);
            mFreeListBitFLI |= (1 << fli);
            mFreeBlock[index] = freeblock;

            freeblock->setNextLink( nullptr );
            freeblock->setPrevLink( nullptr );
        }

        DEBUG_PRINT_BLOCKINFO(freeblock);
    }

    BoundaryTagBegin* mergeFreeBlockPrevNext( BoundaryTagBegin* b )
    {
        DEBUG_PRINT_FUNCNAME();

        BoundaryTagBegin* freeblock = b;
        // merge freelist
        BoundaryTagBegin* next = freeblock->nextTag();
        BoundaryTagEnd* prev_end = reinterpret_cast<BoundaryTagEnd*>( (uint8_t*)b - sizeof(BoundaryTagEnd) );
        
        if( !next->isUsed() ){
            unregisterFreeBlock( next );
            freeblock->merge( next );
        }
        // 一番先頭のタグだったら、前のタグはメモリ範囲外なのでそれをチェック
        if( (uint8_t*)prev_end > mPoolMem ){
            BoundaryTagBegin* prev = freeblock->prevTag();
            assert( mPoolMem <= (uint8_t*)prev && "IT MUST BE BUG" );
            
            if( !prev->isUsed() ){
                unregisterFreeBlock( prev );
                prev->merge( freeblock );
                freeblock = prev;
            }
        }

        return freeblock;
    }

    void deallocateBlock( BoundaryTagBegin* b )
    {
        DEBUG_PRINT_FUNCNAME();

        // nullptr だったら何もしないことを保証
        if( !b ){
            return;
        }

        assert( b->isUsed() && "Double free! IT MUST BE BUG" ); 
        assert( mPoolMem <= (uint8_t*)b && "IT MUST BE BUG" );
        assert( (mPoolMem + mPoolMemSize - sizeof(BoundaryTagBegin)) > (uint8_t*)b && "IT MUST BE BUG" );

        BoundaryTagBegin* block = b;

        block->setUsed( false );
        block = mergeFreeBlockPrevNext( block );
        registerFreeBlock( block );
    }
    
    static const uint32_t skAlignment = 4;   //! memory alignment 
    
    uint32_t mSLIPartitionBits;              //! SLIのビット数
    uint32_t mSLIPartition;
    uint32_t mMinAllocSize;                  //! メモリ確保するときの最小値 これはSLIの分割数でもある
    
    uint8_t* mPoolMem;                          //! pool area
    uint32_t mPoolMemSize;                      //! pool の領域サイズ
    std::vector<BoundaryTagBegin*> mFreeBlock;  //! freeblock list
    uint32_t mFreeListBitFLI;                   //! FLI の freelist flag
    std::vector<uint32_t> mFreeListBitSLI;      //! SLI の freelist flag
};
}

#endif   // EXLIB_TLSF_ALLOCATOR_IMPL_HPP


