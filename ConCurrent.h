#pragma once
#include"common.h"
//可以直接去堆上申请空间，通过系统调用接口





//定长内存池
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		if (_freeList) {//优先把还回来的返回给内存对象重复利用
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
		}
		else {
			//剩余的内存不够一个对象大小时，重新开辟新的空间，进入if
			if (_remainBytes < sizeof(T)) {
				_memory = (char*)malloc(128 * 1024); //也可以直接问系统去要，不用malloc，直接用系统调用接口去调用
				_remainBytes = 128 * 1024;
				if (!_memory) {
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
		}

		//定位new，显示调用T的构造函数初始化
		new(obj)T;
		return obj;
	}
	void Delete(T* obj) {
		obj->~T();
		//if (_freeList == nullptr) {//可以不用判断，两种方式都可以用
		//	_freeList = obj;
		//	*(void**)obj = nullptr;//这里如何解决是关键！
		//}
		//else {//头插
			*(void**)obj = _freeList;
			_freeList = obj;
		//}
	}
private:
	char* _memory = nullptr;//用char是因为刚好char是一个字节，这样在切分的时候，我们向后移动的时候直接加上数字就可以了
	//指向大块内存的指针

	//切分过程中的剩余字节数
	size_t _remainBytes = 0;
	//那申请后怎么去管理呢？
	//我们用一个自由链表来管理
	void* _freeList = nullptr;

};

//template<size_t N>
//class ObjectPool {
//
//};
//定长内存池