#include "Exlib_TLSFAllocator.hpp"

#include <random>
#include <iostream>
#include <cstring>
#include <chrono>
#include <cassert>

//
// �������茾���Ə��������_���Ȋ����ŕW����new�ƃJ�X�^���A���P�[�^�̑��x���r����v���O����
// 

// �W���� new �� �J�X�^���A���P�[�^�ǂ��炩��m�ۂ��邩
#define   ALLOC_CUSTOM_ALLOCATOR

int main( int argc, char** argv )
{
    exlib::TLSF_Allocator allocator( 5*1024*1024 );    // �J�X�^���A���P�[�^�̗̈� 5MB
    std::vector<char*> p_list( 5120, nullptr );
    std::random_device rd;
    std::mt19937 mt(852);

    std::uniform_int_distribution<unsigned> dice(1,32);    // �m�ۂ���o�C�g���� 1-32byte�܂�
    auto start = std::chrono::system_clock::now();

    int i = 0;
    uint64_t alloc_count = 0;
    int time_limit = 30000;    // 30�b

    while( true ){
        auto end = std::chrono::system_clock::now();
        auto span = end - start;
        if( std::chrono::duration_cast<std::chrono::milliseconds>(span).count() >= time_limit )
        {
            break;
        }
        ++alloc_count;

        // 3���1�񂭂炢�͉�����Ă݂�
        int alloc_size = dice(mt);
        if( alloc_size % 3 == 0 && i > 0 ){
            --i;
            // std::cout << alloc_size << ' ' << "dealloc : " << (int*)p_list[i] << ' ' << alloc_size << std::endl;
#ifdef   ALLOC_CUSTOM_ALLOCATOR
            allocator.deallocate( p_list[i] );
#else
	    delete [] p_list[i];
#endif
            continue;
        }
        
        //std::cout << "allocate No: " << i << ' ' << alloc_size << std::endl;
#ifdef   ALLOC_CUSTOM_ALLOCATOR
        p_list[i] = reinterpret_cast<char*>( allocator.allocate(alloc_size) );
#else
        p_list[i] = new char[alloc_size];
#endif
        if( p_list[i] ){
            std::memset( p_list[i], 'A', alloc_size );
        }

        // �������̋󂫂��Ȃ��m�ۂɎ��s�������A���������X�g�����ς��Ȃ��Ăɉ��
        if( !p_list[i] || i >= 5119 ){
            //std::cout << "deallocate all " << std::endl;
            //allocator.printMemoryList();
            for( int j = 0; j <= i; ++j ){
                //std::cout << "dealloc : " << (int*)p_list[j]  << ' ' << j << std::endl;
#ifdef   ALLOC_CUSTOM_ALLOCATOR
                allocator.deallocate( p_list[j] );
#else
                delete [] p_list[j];
#endif
            }
            i = 0;
            continue;
        }
        
        ++i;
    }

    //allocator.printMemoryList();
    std::cout << "Allocation count : " << alloc_count << std::endl;

    return 0;
}
