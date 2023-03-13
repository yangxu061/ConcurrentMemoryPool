#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);

	size_t index = SizeClass::Index(size);

	if (!_freeLists[index].empty())
	{
		return _freeLists[index].Pop();
	}
	else
	{
		//向上对齐后的空间大小
		size_t alignSize = SizeClass::RoundUp(size);
		return FetchFromCentralCache(index, alignSize);
	}
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;

	//慢增长，从1~其最大个数
	size_t num = _freeLists[index].BatchNum();
	if (num < SizeClass::MaxNum(size))
	{
		_freeLists[index].BatchNum()++;
	}

	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, num, size);
	assert(actualNum >= 1);

	if (actualNum == 1)
	{
		assert(start == end);
		return start;
	}
	else
	{
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size);

	size_t index = SizeClass::Index(size);

	_freeLists[index].Push(ptr);

	//检查是否存在大量未使用的小内存块，如果是，则批量返回给CT
	// 当链表长度大于一次批量申请的内存时就开始还一段list给central cache
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
	{
		//size_t alignSize = SizeClass::RoundUp(size);//向上对齐后的空间大小
		//ListTooLong(_freeLists[index], alignSize);
		ListTooLong(_freeLists[index], size);
	}
}
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());

	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}



