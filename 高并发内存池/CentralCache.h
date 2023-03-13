#pragma once

#include "Common.h"

class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	// ��ȡһ���ǿյ�span
	Span* GetOneSpan(SpanList& list, size_t size);

	// �����Ļ����ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	//��TC���������ڴ����¹��ڶ�Ӧ��span��
	void ReleaseListToSpans(void* start, size_t size);

private:
	SpanList _freeLists[FREELIST_NUM];
private:
	CentralCache() {};
	CentralCache(const CentralCache&) = delete;

	static CentralCache _sInst;//����ģʽ
};