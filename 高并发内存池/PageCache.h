#pragma once

#include "Common.h"
#include <unordered_map>
#include <map>
#include "ObjectPool.h"
#include "PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	// 获取一个K页的span
	Span* NewSpan(size_t k);

	// 获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);

	//将CC还回来的Span挂回来
	void ReleaseSpanToPageCache(Span* span);
public:
	std::mutex _pageMtx;
private:
	SpanList _pageLists[PAGELIST_NUM];
	ObjectPool<Span> _spanPool;
	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;//从PageId到存储其内容的Span*的映射
	//std::map<PAGE_ID, Span*> _idSpanMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

private:
	PageCache() {};
	PageCache(const PageCache&) = delete;

	static PageCache _sInst;
};
