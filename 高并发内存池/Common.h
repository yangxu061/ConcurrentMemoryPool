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
	typedef unsigned long long PAGE_ID;	//64λ�£���2^64��С�Ŀռ䣬һҳ2^13����λ����ҳ�������2^51
#elif _WIN32
	typedef size_t PAGE_ID;				//����size_t 2^16(4�ֽ� 32λ��)��2^32(8�ֽ� 64λ��)
#else
	// linux
#endif

static const size_t FREELIST_NUM = 208;		//TC CC ��ṹ��ϣͰ�ĸ��� [0,207] <--> �±�
static const size_t MAX_BYTES = 256 * 1024;	//������������
static const size_t PAGELIST_NUM = 129;		//PC ���ϣͰ�ĸ�����[0,128] <--> ҳ��
static const size_t PAGE_SHIFT = 13;		//һҳ��2^13���ֽڴ�С


// ֱ��ȥ���ϰ�ҳ����ռ䣬����kpageҳ
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux��brk mmap��
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
	// sbrk unmmap��
#endif
}

static inline void*& NextObj(void* obj)
{
	//obj��ָ��ռ��ǰsizeof(void*)��С�ռ������
	return *(void**)obj;	
}

class FreeList
{
public:
	void Push(void* obj)//ͷ��
	{
		NextObj(obj) = _head;
		_head = obj;
		++_size;
	}
	//ͷ����
	void PushRange(void* start, void* end, size_t n)
	{
		NextObj(end) = _head;
		_head = start;
		_size += n;
	}

	void* Pop()//ͷɾ
	{
		assert(_head);
		assert(_size);

		void* obj = _head;
		_head = NextObj(_head);

		_size--;
		return obj;
	}
	//ɾ��ǰ���    start��end������Ͳ���
	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= _size);

		start = _head;
		end = start;

		size_t i = 1;
		while (i < n)//end����ƶ�n���ڵ�
		{
			end = NextObj(end);
			++i;
		}

		_head = NextObj(end);//��������ȥ��
		NextObj(end) = nullptr;

		_size -= n;
	}

	bool empty()
	{
		return _head == nullptr;
	}

	size_t Size()//���ص�ǰ�ڵ�����
	{
		return _size;
	}

	size_t MaxSize()//����FreeList����ŵ��������
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
	size_t _count = 1;//һ�λ�ȡС�ռ�ĸ���
	size_t _size = 0; //FreeList�нڵ�ĸ���
};

class SizeClass
{
public:
	// ������������10%���ҵ�����Ƭ�˷�
	// [1,128] 8byte���� freelist[0,16)
	// [128+1,1024] 16byte���� freelist[16,72)
	// [1024+1,8*1024] 128byte���� freelist[72,128)
	// [8*1024+1,64*1024] 1024byte���� freelist[128,184)
	// [64*1024+1,256*1024] 8*1024byte���� freelist[184,208)

	//����bytes���϶����Ĵ�С������Ƭ������
	static size_t _RoundUp(size_t bytes, size_t alignNum)
	{
		//[1,8]-->8	[9,16]-->16	[17,24]-->24
		//[129,144]-->144	����
		
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
			return _RoundUp(bytes, 1 << PAGE_SHIFT);//��һҳ�Ĵ�СΪ������
			/*assert(false);
			return -1;*/
		}
	}

	//����bytes��TC��ϣͰ���±�[0,208)
	static size_t _Index(size_t bytes, size_t align_shift)
	{
		//[1,8]-->0	[9,16]-->1	[17,24]-->2  
		//[129,144]-->16	����
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

	//���ڼ���bytes��С�Ŀռ�һ��������뼸��
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

	// ����һ����ϵͳ��ȡ����ҳ
	// �������� 8byte
	// ...
	// �������� 256KB
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
	PAGE_ID _pageId = 0; // ҳ��
	size_t _n = 0; // ҳ������

	Span* _prev = nullptr;
	Span* _next = nullptr;

	void* _freeList = nullptr;  // �кõ�С���ڴ����������
	size_t _useCount = 0; // �к�С���ڴ棬�������thread cache�ļ���

	bool _isUse = false;//��ʾ��span�Ƿ���ʹ��״̬
	size_t _objSize = 0;//��span�е�С����Ĵ�С
};

class SpanList
{
public:
	////��ͷ˫��ѭ�������ʼ��
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
	//��posǰ����newSpan
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
	//������ȥ��span
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

	void PushFront(Span* span)//ͷ��
	{
		Insert(Begin(), span);
	}

	Span* PopFront()//ͷɾ
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
	std::mutex _spanListMtx; // Ͱ��
};

