#include "PageCache.h"


PageCache PageCache::_sInst;

// ��ȡһ��Kҳ��span
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);

	if (k > PAGELIST_NUM - 1)//�������ҳ����ֱ���������
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
		Span* kSpan = _pageLists[k].PopFront();//ֱ�ӽ�span�и�CC

		// ����id��span��ӳ�䣬����central cache����С���ڴ�ʱ�����Ҷ�Ӧ��span
		for (PAGE_ID i = 0; i < kSpan->_n; ++i)
		{
			//_idSpanMap[kSpan->_pageId + i] = kSpan;
			_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}

		return kSpan;
	}
	
	//�������<kҳ��span����i�У����зֳ� k��i-k����span
	for (size_t i = k + 1; i < PAGELIST_NUM; ++i)
	{
		if (!_pageLists[i].empty())
		{
			Span* iSpan = _pageLists[i].PopFront();
			Span* kSpan = _spanPool.New();

			//��iSpanͷ����һ��kSpan
			kSpan->_n = k;
			kSpan->_pageId = iSpan->_pageId;//_pageId��ʶ��ַ

			iSpan->_pageId += k;
			iSpan->_n -= k;

			_pageLists[iSpan->_n].PushFront(iSpan);

			//�洢 �µ�δʹ�õ�iSan��ǰ����ҳid<-->Span*��ӳ���ϵ
			/*_idSpanMap[iSpan->_pageId] = iSpan;
			_idSpanMap[iSpan->_pageId + iSpan->_n - 1] = iSpan;*/
			_idSpanMap.set(iSpan->_pageId, iSpan);
			_idSpanMap.set(iSpan->_pageId + iSpan->_n - 1, iSpan);



			// ����id��span��ӳ�䣬����central cache����С���ڴ�ʱ�����Ҷ�Ӧ��span
			for (PAGE_ID i = 0; i < kSpan->_n; ++i)
			{
				//_idSpanMap[kSpan->_pageId + i] = kSpan;
				_idSpanMap.set(kSpan->_pageId + i, kSpan);

			}

			return kSpan;
		}
	}

	// �ߵ����λ�þ�˵������û�д�ҳ��span��
	// ��ʱ��ȥ�Ҷ�Ҫһ��128ҳ�����һ������span
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(PAGELIST_NUM - 1);
	
	bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;

	bigSpan->_n = PAGELIST_NUM - 1;

	_pageLists[bigSpan->_n].PushFront(bigSpan);

	return NewSpan(k);//���븴��

}

// ��ȡ�Ӷ���span��ӳ��
Span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;//obj����ҳ��ҳ��

	//std::unique_lock<std::mutex> lock(_pageMtx);//��������ӳ���ϵ

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

//��CC��������Span�һ���
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//����ڴ��ͷ�
	if (span->_n > PAGELIST_NUM - 1)
	{
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);

		return;
	}

	while (1) //��ǰ�ϲ�
	{
		PAGE_ID pId = span->_pageId - 1;
		/*auto ret = _idSpanMap.find(pId);
		
		if (ret == _idSpanMap.end()) break;		*///û�и�spanid

		auto ret = (Span*)_idSpanMap.get(pId);
		if (ret == nullptr)
		{
			break;
		}

		//Span* prevSpan = ret->second;
		Span* prevSpan = ret;
		if (prevSpan->_isUse == true) break;	//��ʹ��

		if (span->_n + prevSpan->_n > PAGELIST_NUM - 1) break;	//�������Χ

		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		_pageLists[prevSpan->_n].Erase(prevSpan);
		//delete prevSpan;
		_spanPool.Delete(prevSpan);
	}

	while (1) //���ϲ�
	{
		PAGE_ID nId = span->_pageId + span->_n;
		//auto ret = _idSpanMap.find(nId);

		//if (ret == _idSpanMap.end()) break;//������
		auto ret = (Span*)_idSpanMap.get(nId);
		if (ret == nullptr)
		{
			break;
		}

		//Span* nextSpan = ret->second;
		Span* nextSpan = ret;

		if (nextSpan->_isUse == true) break;//��ʹ����

		if (span->_n + nextSpan->_n > PAGELIST_NUM - 1) break;//�ϲ���ҳ����������޶�

		//span->_pageId = nextSpan->_pageId;
		span->_n += nextSpan->_n;

		_pageLists[nextSpan->_n].Erase(nextSpan);
		//delete nextSpan;
		_spanPool.Delete(nextSpan);
	}

	//�ϲ�����
	_pageLists[span->_n].PushFront(span);
	span->_isUse = false;
	//�洢 �µ�δʹ�õ�span��ǰ����ҳid<-->Span*��ӳ���ϵ
	/*_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId + span->_n - 1] = span;*/
	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);
}