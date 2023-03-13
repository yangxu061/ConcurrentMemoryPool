#include "PageCache.h"


PageCache PageCache::_sInst;

// 获取一个K页的span
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);

	if (k > PAGELIST_NUM - 1)//超过最大页数，直接向堆申请
	{
		void* ptr = SystemAlloc(k);
		//Span* span = new Span();
		Span* span = _spanPool.New();

		span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;

		span->_n = k;
		span->_isUse = true;

		//_idSpanMap[span->_pageId] = span;
		_idSpanMap.set(span->_pageId, span);

		return span;
	}

	if (!_pageLists[k].empty())
	{
		Span* kSpan = _pageLists[k].PopFront();//直接将span切给CC

		// 建立id和span的映射，方便central cache回收小块内存时，查找对应的span
		for (PAGE_ID i = 0; i < kSpan->_n; ++i)
		{
			//_idSpanMap[kSpan->_pageId + i] = kSpan;
			_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}

		return kSpan;
	}
	
	//检查有无<k页的span，若i有，则切分成 k和i-k两个span
	for (size_t i = k + 1; i < PAGELIST_NUM; ++i)
	{
		if (!_pageLists[i].empty())
		{
			Span* iSpan = _pageLists[i].PopFront();
			Span* kSpan = _spanPool.New();

			//从iSpan头切下一个kSpan
			kSpan->_n = k;
			kSpan->_pageId = iSpan->_pageId;//_pageId标识地址

			iSpan->_pageId += k;
			iSpan->_n -= k;

			_pageLists[iSpan->_n].PushFront(iSpan);

			//存储 新的未使用的iSan的前后两页id<-->Span*的映射关系
			/*_idSpanMap[iSpan->_pageId] = iSpan;
			_idSpanMap[iSpan->_pageId + iSpan->_n - 1] = iSpan;*/
			_idSpanMap.set(iSpan->_pageId, iSpan);
			_idSpanMap.set(iSpan->_pageId + iSpan->_n - 1, iSpan);



			// 建立id和span的映射，方便central cache回收小块内存时，查找对应的span
			for (PAGE_ID i = 0; i < kSpan->_n; ++i)
			{
				//_idSpanMap[kSpan->_pageId + i] = kSpan;
				_idSpanMap.set(kSpan->_pageId + i, kSpan);

			}

			return kSpan;
		}
	}

	// 走到这个位置就说明后面没有大页的span了
	// 这时就去找堆要一个128页（最后一个）的span
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(PAGELIST_NUM - 1);
	
	bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;

	bigSpan->_n = PAGELIST_NUM - 1;

	_pageLists[bigSpan->_n].PushFront(bigSpan);

	return NewSpan(k);//代码复用

}

// 获取从对象到span的映射
Span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;//obj所在页的页号

	//std::unique_lock<std::mutex> lock(_pageMtx);//加锁访问映射关系

	//auto ret = _idSpanMap.find(id);

	//if (ret != _idSpanMap.end())
	//{
	//	return ret->second;
	//}
	//else
	//{
	//	assert(false);
	//	return nullptr;
	//}

	auto ret = (Span*)_idSpanMap.get(id);
	assert(ret != nullptr);
	return ret;
}

//将CC还回来的Span挂回来
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//大块内存释放
	if (span->_n > PAGELIST_NUM - 1)
	{
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);

		return;
	}

	while (1) //向前合并
	{
		PAGE_ID pId = span->_pageId - 1;
		/*auto ret = _idSpanMap.find(pId);
		
		if (ret == _idSpanMap.end()) break;		*///没有该spanid

		auto ret = (Span*)_idSpanMap.get(pId);
		if (ret == nullptr)
		{
			break;
		}

		//Span* prevSpan = ret->second;
		Span* prevSpan = ret;
		if (prevSpan->_isUse == true) break;	//被使用

		if (span->_n + prevSpan->_n > PAGELIST_NUM - 1) break;	//超过最大范围

		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		_pageLists[prevSpan->_n].Erase(prevSpan);
		//delete prevSpan;
		_spanPool.Delete(prevSpan);
	}

	while (1) //向后合并
	{
		PAGE_ID nId = span->_pageId + span->_n;
		//auto ret = _idSpanMap.find(nId);

		//if (ret == _idSpanMap.end()) break;//不存在
		auto ret = (Span*)_idSpanMap.get(nId);
		if (ret == nullptr)
		{
			break;
		}

		//Span* nextSpan = ret->second;
		Span* nextSpan = ret;

		if (nextSpan->_isUse == true) break;//在使用中

		if (span->_n + nextSpan->_n > PAGELIST_NUM - 1) break;//合并后页数超过最大限度

		//span->_pageId = nextSpan->_pageId;
		span->_n += nextSpan->_n;

		_pageLists[nextSpan->_n].Erase(nextSpan);
		//delete nextSpan;
		_spanPool.Delete(nextSpan);
	}

	//合并结束
	_pageLists[span->_n].PushFront(span);
	span->_isUse = false;
	//存储 新的未使用的span的前后两页id<-->Span*的映射关系
	/*_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId + span->_n - 1] = span;*/
	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);
}