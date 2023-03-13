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

	// ��ȡһ��Kҳ��span
	Span* NewSpan(size_t k);

	// ��ȡ�Ӷ���span��ӳ��
	Span* MapObjectToSpan(void* obj);

	//��CC��������Span�һ���
	void ReleaseSpanToPageCache(Span* span);
public:
	std::mutex _pageMtx;
private:
	SpanList _pageLists[PAGELIST_NUM];
	ObjectPool<Span> _spanPool;
	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;//��PageId���洢�����ݵ�Span*��ӳ��
	//std::map<PAGE_ID, Span*> _idSpanMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

private:
	PageCache() {};
	PageCache(const PageCache&) = delete;

	static PageCache _sInst;
};
