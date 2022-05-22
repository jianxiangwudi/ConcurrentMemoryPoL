#pragma once
#include"common.h"

//因为是在所有的线程共享的，所以需要设计为单例模式
//我们设计为饿汉模式
class CentralCache {
public:
	static CentralCache* GetInstace() {
		return &_sInst;
	}

	//获取一个非空的span
	Span* GetOneSpan(SpanList& list, size_t byte_size);
	//从中心缓存中获取一定数量的对象给thread_cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t byte_size);

	//还回去找对应的span
	void ReleaseListToSpans(void* start, size_t size);
private:
	CentralCache()
	{}

	CentralCache(const CentralCache&) = delete;
	SpanList _spanLists[NFREELIST];
	static CentralCache _sInst;
};