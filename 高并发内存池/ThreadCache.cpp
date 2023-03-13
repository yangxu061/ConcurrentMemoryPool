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
		//���϶����Ŀռ��С
		size_t alignSize = SizeClass::RoundUp(size);
		return FetchFromCentralCache(index, alignSize);
	}
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;

	//����������1~��������
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

	//����Ƿ���ڴ���δʹ�õ�С�ڴ�飬����ǣ����������ظ�CT
	// �������ȴ���һ������������ڴ�ʱ�Ϳ�ʼ��һ��list��central cache
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
	{
		//size_t alignSize = SizeClass::RoundUp(size);//���϶����Ŀռ��С
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



