#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)//在ThreadCache中的内存申请有最大内存限制
	{
		size_t alignSize = SizeClass::RoundUp(size);
		size_t k = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(k);
		span->_isUse = true;//！！！可能是申请一个_n:[32,128]的
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
