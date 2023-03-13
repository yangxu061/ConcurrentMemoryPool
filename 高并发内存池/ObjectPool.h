#pragma once

#include <iostream>
#include <vector>

using std::vector;
using std::cout;
using std::endl;

//用于申请每次类型T字节大小的定长内存池
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		if (_freeList)
		{
			obj = (T*)_freeList;
			_freeList = *(void**)_freeList;

			//void* next = *((void**)_freeList);
			//obj = (T*)_freeList;
			//_freeList = next;
		}
		else
		{
			if (_remainBytes < sizeof(T))//申请整块空间
			{
				_remainBytes = 128 * 1024;
				_memory = (char*)SystemAlloc(_remainBytes >> 13);
				if (_memory == nullptr)
				{
					//throw std::bad_alloc();
					assert(false);
				}
			}
			obj = (T*)_memory;
			//规定最小生成一个指针大小的空间
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);

			_memory += objSize;
			_remainBytes -= objSize;
		}
		
		new(obj)T;	//定位new初始化

		return obj;
	}

	void Delete(T* obj)
	{
		obj->~T();	//先析构

		*(void**)obj = _freeList;
		//_freeList = (void*)obj;
		_freeList = obj;
	}

private:
	char* _memory = nullptr;	//大块总的内存
	void* _freeList = nullptr;	//小块释放的内存
	size_t _remainBytes = 0;	//剩余的内存字节数
};

//struct Dog
//{
//private:
//	int age;
//	double weight;
//	double length;
//};
//void testObjectPool1()
//{
//	ObjectPool<Dog> pool;
//	const int N = 10;
//	vector<Dog*> vd(N,nullptr);
//
//	for (int i = 0; i < N; ++i)
//	{
//		vd[i] = pool.New();
//	}
//	for (int i = 0; i < N / 2; ++i)
//	{
//		pool.Delete(vd[i]);
//		vd[i] = nullptr;
//	}
//	for (int i = 0; i < N / 2; ++i)
//	{
//		vd[i] = pool.New();
//	}
//	for (int i = 0; i < N; ++i)
//	{
//		pool.Delete(vd[i]);
//		vd[i] = nullptr;
//	}
//	cout << "finish" << endl;
//}
//
//
//struct TreeNode
//{
//	int _val;
//	TreeNode* _left;
//	TreeNode* _right;
//
//	TreeNode()
//		:_val(0)
//		, _left(nullptr)
//		, _right(nullptr)
//	{}
//};
//
////release下试
//void TestObjectPool2()
//{
//	// 申请释放的轮次
//	const size_t Rounds = 5;
//
//	// 每轮申请释放多少次
//	const size_t N = 1000000;
//
//	std::vector<TreeNode*> v1;
//	v1.reserve(N);
//
//	size_t begin1 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v1.push_back(new TreeNode);
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			delete v1[i];
//		}
//		v1.clear();
//	}
//
//	size_t end1 = clock();
//
//	std::vector<TreeNode*> v2;
//	v2.reserve(N);
//
//	ObjectPool<TreeNode> TNPool;
//	size_t begin2 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v2.push_back(TNPool.New());
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			TNPool.Delete(v2[i]);
//		}
//		v2.clear();
//	}
//	size_t end2 = clock();
//
//	cout << "new cost time:" << end1 - begin1 << endl;
//	cout << "object pool cost time:" << end2 - begin2 << endl;
//}