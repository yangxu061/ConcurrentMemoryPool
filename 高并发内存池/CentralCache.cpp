#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

// 获取一个非空的span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size) 
{
	//遍历一遍SpanList，寻找有内存的span
	Span* span = list.Begin();
	while (span != list.End())
	{
		if (span->_freeList != nullptr)
		{
			return span;//找到则直接返回
		}
		else
		{
			span = span->_next;
		}
	}

	//没有空间，则需要向PageCache申请一个span

	list._spanListMtx.unlock();//桶锁，解锁		
	//对于其他线程而言，可以进入该桶（进行释放资源 或 向下申请span阻塞在_pageMtx锁竞争上）

	PageCache::GetInstance()->_pageMtx.lock();//总锁，加锁
	////调用PageCache对象的方法申请Span
	span = PageCache::GetInstance()->NewSpan(SizeClass::SizeToPageNum(size));
	span->_isUse = true;//置为使用状态
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();//总锁，解锁

	//将span切分成内存块，并挂起,以顺序切
	char* start = (char*)(span->_pageId << PAGE_SHIFT);//设置为char*，便于指针后移
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;
	
	//|tail|
	//		|start|			|	|end
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	size_t number = 1;
	while (start < end)
	{
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += size;
		++number;
	}

	NextObj(tail) = nullptr;

	list._spanListMtx.lock();//桶锁，加锁
	list.PushFront(span);//把newSpan挂到CC上

	return span;
}

// 从中心缓存获取一定数量的对象给thread cache
//start,end：输出型参数
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);

	_freeLists[index]._spanListMtx.lock();	//桶锁，加锁
	//获取一个对应的span对象
	Span* span = GetOneSpan(_freeLists[index], size);

	size_t num = 1;
	start = span->_freeList;
	end = start;

	//end向后移动batchNum-1个或到空
	//|start|	|end|
	while (num < batchNum && NextObj(end))
	{
		end = NextObj(end);
		++num;
	}

	span->_useCount += num;//使用num个小内存块

	span->_freeList = NextObj(end);//从span的链表中去除
	NextObj(end) = nullptr;
	
	_freeLists[index]._spanListMtx.unlock();	//桶锁，解锁

	return num;//返回实际申请的个数
}

//将TC还回来的内存块重新挂在对应的span上
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);

	_freeLists[index]._spanListMtx.lock();	//桶锁，加锁
	while (start)
	{
		void* next = NextObj(start);//下一个内存块
		//调用PageCache对象的方法得到start所属的span对象地址
		Span* curSpan = PageCache::GetInstance()->MapObjectToSpan(start);

		//将start头插到_freeList上
		NextObj(start) = curSpan->_freeList;	
		curSpan->_freeList = start;
		curSpan->_useCount--;

		if (curSpan->_useCount == 0)//说明curSpan所有的内存块都被还回来了
		{
			_freeLists[index].Erase(curSpan);//将curSpan从链表中去除
			curSpan->_next = curSpan->_prev = nullptr;
			curSpan->_freeList = nullptr;
			//curSpan->_isUse = false;//!!!错误，有可能另一个线程刚申请，此时的_useCount==0，有置为false，在合并的时候会符合条件

			_freeLists[index]._spanListMtx.unlock();//桶锁，解锁

			PageCache::GetInstance()->_pageMtx.lock();//！！！总锁，加锁下
				//调用PageCache对象的方法回收span对象
			PageCache::GetInstance()->ReleaseSpanToPageCache(curSpan);//！！！总锁，加锁下
			PageCache::GetInstance()->_pageMtx.unlock();

			_freeLists[index]._spanListMtx.lock();//桶锁，加锁
		}

		start = next;	
	}

	_freeLists[index]._spanListMtx.unlock();//桶锁，解锁
}