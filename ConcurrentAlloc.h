#pragma once
#include"common.h"
#include"ThreadCache.h"
#include"PageCache.h"
#include"ConCurrent.h"
static void* ConcurrentAlloc(size_t size) {//size = 16
	if (size > MAX_BYTES) {
		size_t alignSize = SizeClass::RoundUp(size);  
		size_t kpage = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kpage);
		span->_objSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		return ptr;
	}
	else {
		if (pTLSThreadCache == nullptr) {
			static ObjectPool<ThreadCache> tcPool;
			//pTLSThreadCache = new ThreadCache;
			pTLSThreadCache = tcPool.New();

		}

		//通过TLS每个线程无锁的获取自己的ThreadCache对象
		//cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;
		return pTLSThreadCache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr) {
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size= span->_objSize;
	if (size > MAX_BYTES) {
		Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);

		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();

	}
	else {
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}