#pragma once
#include "common.h"




//我们知道，每一个线程在创建的时候，都会独享一份ThreadCache，那么其又是怎么创建的呢？
//我们知道，一个进程中的每一个线程共享一份进程地址空间，其中，有一些内容是私有的，比如堆栈指针（栈），上下文信息（寄存器）；而有些东西是共享的
//比如全局变量、静态变量（全局区、代码段里面的内容）等等

//我们现在需要思考的是：有这么多线程和ThreadCache，我应该给哪一个线程创建呢？如何做到申请创建的时候不去加锁？
//到底哪一个线程用的是哪一个ThreadCache并且保证自己是独享的呢等等问题。
class ThreadCache {
public:
	//申请和释放空间
	void* Allocate(size_t size);

	void Deallocate(void* ptr, size_t size);


	void* FetchFromCentralCache(size_t index, size_t size);

	void ListTooLong(FreeList& list, size_t size);

	
private:
	FreeList _freeLists[NFREELIST];
};


//为什么加了static可以保证只在当前文件可见？
//TLS thread local storage
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;