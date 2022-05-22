#pragma once
#include"PageMap.h"
#include"common.h"
#include"ConCurrent.h"
class PageCache {
public:
	static PageCache* GetInstance() {
		return &_sInst;
	}

	//获取一个k页的span
	Span* NewSpan(size_t k);
	std::mutex _pageMtx;
	Span* MapObjectToSpan(void* obj);
	void ReleaseSpanToPageCache(Span* span);

private:
	SpanList _spanLists[NPAGES];
	ObjectPool<Span> spanPool;
	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	//std::unordered_map<PAGE_ID, size_t> _idSizeMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;
	PageCache(){}
	PageCache(const PageCache&) = delete;

	static PageCache _sInst;
};