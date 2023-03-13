#pragma once

#include <iostream>
#include <vector>
#include <assert.h>

#include <thread>
#include <mutex>

#include <algorithm>

using std::vector;
using std::cout;
using std::endl;

#ifdef _WIN32
	#include<windows.h>
#else
// 
#endif

#ifdef _WIN64
	typedef unsigned long long PAGE_ID;	//64位下，有2^64大小的空间，一页2^13比特位，则页号最大有2^51
#elif _WIN32
	typedef size_t PAGE_ID;				//其中size_t 2^16(4字节 32位下)；2^32(8字节 64位下)
#else
	// linux
#endif

static const size_t FREELIST_NUM = 208;		//TC CC 其结构哈希桶的个数 [0,207] <--> 下标
static const size_t MAX_BYTES = 256 * 1024;	//申请的最大上限
static const size_t PAGELIST_NUM = 129;		//PC 其哈希桶的个数，[0,128] <--> 页数
static const size_t PAGE_SHIFT = 13;		//一页有2^13个字节大小


// 直接去堆上按页申请空间，申请kpage页
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}

static inline void*& NextObj(void* obj)
{
	//obj所指向空间的前sizeof(void*)大小空间的数据
	return *(void**)obj;	
}

class FreeList
{
public:
	void Push(void* obj)//头插
	{
		NextObj(obj) = _head;
		_head = obj;
		++_size;
	}
	//头插多个
	void PushRange(void* start, void* end, size_t n)
	{
		NextObj(end) = _head;
		_head = start;
		_size += n;
	}

	void* Pop()//头删
	{
		assert(_head);
		assert(_size);

		void* obj = _head;
		_head = NextObj(_head);

		_size--;
		return obj;
	}
	//删除前多个    start与end是输出型参数
	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= _size);

		start = _head;
		end = start;

		size_t i = 1;
		while (i < n)//end向后移动n个节点
		{
			end = NextObj(end);
			++i;
		}

		_head = NextObj(end);//从链表上去除
		NextObj(end) = nullptr;

		_size -= n;
	}

	bool empty()
	{
		return _head == nullptr;
	}

	size_t Size()//返回当前节点数量
	{
		return _size;
	}

	size_t MaxSize()//返回FreeList所存放的最多数量
	{
		//return BatchNum();
		return _count;
	}

	size_t& BatchNum()
	{
		return _count;
	}

private:
	void* _head = nullptr;
	size_t _count = 1;//一次获取小空间的个数
	size_t _size = 0; //FreeList中节点的个数
};

class SizeClass
{
public:
	// 整体控制在最多10%左右的内碎片浪费
	// [1,128] 8byte对齐 freelist[0,16)
	// [128+1,1024] 16byte对齐 freelist[16,72)
	// [1024+1,8*1024] 128byte对齐 freelist[72,128)
	// [8*1024+1,64*1024] 1024byte对齐 freelist[128,184)
	// [64*1024+1,256*1024] 8*1024byte对齐 freelist[184,208)

	//计算bytes向上对齐后的大小（内碎片产生）
	static size_t _RoundUp(size_t bytes, size_t alignNum)
	{
		//[1,8]-->8	[9,16]-->16	[17,24]-->24
		//[129,144]-->144	……
		
		//if (bytes % alignNum == 0)
		//	return bytes;
		//else
		//	return (bytes / alignNum + 1) * alignNum;
		
		return ((bytes + alignNum - 1) & ~(alignNum - 1));
	}

	static size_t RoundUp(size_t bytes)
	{
		if (bytes <= 128)
		{
			return _RoundUp(bytes, 8);
		}
		else if (bytes <= 1024)
		{
			return _RoundUp(bytes, 16);
		}
		else if (bytes <= 8 * 1024)
		{
			return _RoundUp(bytes, 128);
		}
		else if (bytes <= 64 * 1024)
		{
			return _RoundUp(bytes, 1024);
		}
		else if (bytes <= 256 * 1024)
		{
			return _RoundUp(bytes, 8 * 1024);
		}
		else
		{
			return _RoundUp(bytes, 1 << PAGE_SHIFT);//按一页的大小为对齐数
			/*assert(false);
			return -1;*/
		}
	}

	//计算bytes在TC哈希桶的下标[0,208)
	static size_t _Index(size_t bytes, size_t align_shift)
	{
		//[1,8]-->0	[9,16]-->1	[17,24]-->2  
		//[129,144]-->16	……
		//if (bytes % (1 << align_shift) == 0)
		//	return bytes / (1 << align_shift) - 1;
		//else
		//	return bytes / (1 << align_shift);

		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}


	static size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128)
		{
			return _Index(bytes, 3); //2^3=8
		}
		else if (bytes <= 1024)
		{
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if (bytes <= 8 * 1024) 
		{
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 64 * 1024) 
		{
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1]
				+ group_array[0];
		}
		else if (bytes <= 256 * 1024) 
		{
			return _Index(bytes - 64 * 1024, 13) + group_array[3] +
				group_array[2] + group_array[1] + group_array[0];
		}
		else 
		{
			assert(false);
			return -1;
		}
	}

	//用于计算bytes大小的空间一次最多申请几个
	static size_t MaxNum(size_t bytes)
	{
		/*if (bytes == 0)
			return 0;
		*/
		assert(bytes > 0);
		int num = MAX_BYTES / bytes;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;

		return num;
	}

	// 计算一次向系统获取几个页
	// 单个对象 8byte
	// ...
	// 单个对象 256KB
	//
	static size_t SizeToPageNum(size_t bytes)
	{
		size_t num = MaxNum(bytes);
		size_t npage = num * bytes;

		npage >>= PAGE_SHIFT;

		if (npage == 0) npage = 1;

		return npage;
	}
};

struct Span
{
	PAGE_ID _pageId = 0; // 页号
	size_t _n = 0; // 页的数量

	Span* _prev = nullptr;
	Span* _next = nullptr;

	void* _freeList = nullptr;  // 切好的小块内存的自由链表
	size_t _useCount = 0; // 切好小块内存，被分配给thread cache的计数

	bool _isUse = false;//表示该span是否是使用状态
	size_t _objSize = 0;//该span中的小对象的大小
};

class SpanList
{
public:
	////带头双向循环链表初始化
	SpanList()
	{
		_head = new Span();
		_head->_prev = _head;
		_head->_next = _head;
	}

	Span* Begin()
	{
		return _head->_next;
	}

	Span* End()
	{
		return _head;
	}
	//在pos前插入newSpan
	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);

		//prev newSpan pos
		Span* prev = pos->_prev;

		prev->_next = newSpan;
		newSpan->_next = pos;
		newSpan->_prev = prev;
		pos->_prev = newSpan;
	}
	//从链表去除span
	void Erase(Span* span)
	{
		assert(span);
		assert(span != _head);

		//prev span next
		Span* prev = span->_prev;
		Span* next = span->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	void PushFront(Span* span)//头插
	{
		Insert(Begin(), span);
	}

	Span* PopFront()//头删
	{
		Span* span = Begin();
		Erase(span);
		return span;
	}

	bool empty()
	{
		return Begin() == End();
	}

private:
	Span* _head;
public:
	std::mutex _spanListMtx; // 桶锁
};

