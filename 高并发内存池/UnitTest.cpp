
//#include "SimplePool.h"
#include "ConcurrentAlloc.h"



void Alloc1()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
	}
}

void Alloc2()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(7);
	}
}

void TLSTest()
{
	std::thread t1(Alloc1);
	t1.join();

	std::thread t2(Alloc2);
	t2.join();
}

void TestConcurrentAlloc1()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(1);
	void* p4 = ConcurrentAlloc(7);
	void* p5 = ConcurrentAlloc(8);

	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;
}

void TestConcurrentAlloc2()
{
	for (size_t i = 0; i < 1024; ++i)
	{
		void* p1 = ConcurrentAlloc(6);
		cout << p1 << endl;
	}

	void* p2 = ConcurrentAlloc(8);
	cout << p2 << endl;
}


void MultiThreadAlloc1()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		if (i == 6)
		{
			int a = 0;
			a++;
		}
		void* ptr = ConcurrentAlloc(6);
		v.push_back(ptr);
	}

	for (size_t i = 0; i < 7; ++i)
	{
		if (i == 6)
		{
			int a = 0;
			a++;
		}
		ConcurrentFree(v[i]);
	}
}

void MultiThreadAlloc2()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(16);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		ConcurrentFree(e);
	}
}

void TestMultiThread()
{
	std::thread t1(MultiThreadAlloc1);
	t1.join();

	std::thread t2(MultiThreadAlloc2);

	t2.join();
}

void TestBigSizeAlloc1()
{
	void* p1 = ConcurrentAlloc(257*1024);
	ConcurrentFree(p1);

	void* p2 = ConcurrentAlloc(129*8*1024);
	ConcurrentFree(p2);
	
}

//int main()
//{
//	/*TestConcurrentAlloc2();
//	TestMultiThread();
//
//	TestBigSizeAlloc1();*/
//	//cout << sizeof(ThreadCache) << endl;
//    return 0;
//}



// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
