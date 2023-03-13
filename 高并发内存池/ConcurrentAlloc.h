#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)//��ThreadCache�е��ڴ�����������ڴ�����
	{
		size_t alignSize = SizeClass::RoundUp(size);
		size_t k = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(k);
		span->_isUse = true;//����������������һ��_n:[32,128]��
		span->_objSize = alignSize;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);

		return ptr;
	}
	else
	{
		if (pTLSThreadCache == nullptr)
		{
			static std::mutex poolMtx;
			static ObjectPool<ThreadCache> tcPool;
			poolMtx.lock();
			if(pTLSThreadCache == nullptr)
				pTLSThreadCache = tcPool.New();
			poolMtx.unlock();
			//pTLSThreadCache = new ThreadCache();
		}

		return pTLSThreadCache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;

	if (size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		span->_isUse = false;//!!!
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		assert(pTLSThreadCache);

		pTLSThreadCache->Deallocate(ptr, size);
	}
}
