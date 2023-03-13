#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

// ��ȡһ���ǿյ�span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size) 
{
	//����һ��SpanList��Ѱ�����ڴ��span
	Span* span = list.Begin();
	while (span != list.End())
	{
		if (span->_freeList != nullptr)
		{
			return span;//�ҵ���ֱ�ӷ���
		}
		else
		{
			span = span->_next;
		}
	}

	//û�пռ䣬����Ҫ��PageCache����һ��span

	list._spanListMtx.unlock();//Ͱ��������		
	//���������̶߳��ԣ����Խ����Ͱ�������ͷ���Դ �� ��������span������_pageMtx�������ϣ�

	PageCache::GetInstance()->_pageMtx.lock();//����������
	////����PageCache����ķ�������Span
	span = PageCache::GetInstance()->NewSpan(SizeClass::SizeToPageNum(size));
	span->_isUse = true;//��Ϊʹ��״̬
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();//����������

	//��span�зֳ��ڴ�飬������,��˳����
	char* start = (char*)(span->_pageId << PAGE_SHIFT);//����Ϊchar*������ָ�����
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

	list._spanListMtx.lock();//Ͱ��������
	list.PushFront(span);//��newSpan�ҵ�CC��

	return span;
}

// �����Ļ����ȡһ�������Ķ����thread cache
//start,end������Ͳ���
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);

	_freeLists[index]._spanListMtx.lock();	//Ͱ��������
	//��ȡһ����Ӧ��span����
	Span* span = GetOneSpan(_freeLists[index], size);

	size_t num = 1;
	start = span->_freeList;
	end = start;

	//end����ƶ�batchNum-1���򵽿�
	//|start|	|end|
	while (num < batchNum && NextObj(end))
	{
		end = NextObj(end);
		++num;
	}

	span->_useCount += num;//ʹ��num��С�ڴ��

	span->_freeList = NextObj(end);//��span��������ȥ��
	NextObj(end) = nullptr;
	
	_freeLists[index]._spanListMtx.unlock();	//Ͱ��������

	return num;//����ʵ������ĸ���
}

//��TC���������ڴ�����¹��ڶ�Ӧ��span��
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);

	_freeLists[index]._spanListMtx.lock();	//Ͱ��������
	while (start)
	{
		void* next = NextObj(start);//��һ���ڴ��
		//����PageCache����ķ����õ�start������span�����ַ
		Span* curSpan = PageCache::GetInstance()->MapObjectToSpan(start);

		//��startͷ�嵽_freeList��
		NextObj(start) = curSpan->_freeList;	
		curSpan->_freeList = start;
		curSpan->_useCount--;

		if (curSpan->_useCount == 0)//˵��curSpan���е��ڴ�鶼����������
		{
			_freeLists[index].Erase(curSpan);//��curSpan��������ȥ��
			curSpan->_next = curSpan->_prev = nullptr;
			curSpan->_freeList = nullptr;
			//curSpan->_isUse = false;//!!!�����п�����һ���̸߳����룬��ʱ��_useCount==0������Ϊfalse���ںϲ���ʱ����������

			_freeLists[index]._spanListMtx.unlock();//Ͱ��������

			PageCache::GetInstance()->_pageMtx.lock();//������������������
				//����PageCache����ķ�������span����
			PageCache::GetInstance()->ReleaseSpanToPageCache(curSpan);//������������������
			PageCache::GetInstance()->_pageMtx.unlock();

			_freeLists[index]._spanListMtx.lock();//Ͱ��������
		}

		start = next;	
	}

	_freeLists[index]._spanListMtx.unlock();//Ͱ��������
}