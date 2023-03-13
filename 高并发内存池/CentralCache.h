#pragma once

#include "Common.h"

class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	// 获取一个非空的span
	Span* GetOneSpan(SpanList& list, size_t size);

	// 从中心缓存获取一定数量的对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	//将TC还回来的内存重新挂在对应的span上
	void ReleaseListToSpans(void* start, size_t size);

private:
	SpanList _freeLists[FREELIST_NUM];
private:
	CentralCache() {};
	CentralCache(const CentralCache&) = delete;

	static CentralCache _sInst;//单例模式
};