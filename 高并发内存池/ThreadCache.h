#pragma once
 
#include "Common.h"

class ThreadCache
{
public:
	void* Allocate(size_t size);

	void Deallocate(void* ptr, size_t size);

	void* FetchFromCentralCache(size_t index, size_t size);

	//链表太长进行回收
	void ListTooLong(FreeList& list, size_t size);

private:
	FreeList _freeLists[FREELIST_NUM];
};

// TLS thread local storage
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;