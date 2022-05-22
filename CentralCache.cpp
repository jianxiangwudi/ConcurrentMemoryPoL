#include"common.h"
#include"CentralCache.h"
#include"PageCache.h"

CentralCache CentralCache::_sInst;

Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size) {

	//查看当前的spanlist中是否还有未被分配对象的span
	Span* it = list.begin();
	while (it != list.end()) {
		if (it->_freeList != nullptr) {
			return it;
		}
		else {
			it = it->_next;
		}
	}

	//先把central cache的桶解掉，这样的话如果其他的线程来释放对象回来，就不会阻塞
	list._mtx.unlock();	
	//走到这里，就说明没有空闲的span了，需要找page_span要
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(byte_size));
	span->isUsed = true;
	span->_objSize = byte_size;
	PageCache::GetInstance()->_pageMtx.unlock();

	//对span进行切分的时候就不需要加锁，因为这个时候其他线程访问不到这个span


	//拿过来了这么多span，通过NewSpan拿到了一个，要对其进行切分。
	//我们通过页号，计算页的起始地址和内存的大小（以字节为单位）
	char* start = (char*)(span->_pageid << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;

	//下面把大块的内存切成自由链表挂起来，这里是切到了Thread_cache里面
	span->_freeList = start;
	start += byte_size;
	void* tail = span->_freeList;
	while (start < end) {
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += byte_size;
	}

	NextObj(tail) = end;
	NextObj(end) = nullptr;
	//这里将切好的span挂到桶里面的时候再加锁
	list._mtx.lock();
	list.PushFront(span);

	return span;
}

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size) {
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	Span* span = GetOneSpan(_spanLists[index],size);
	assert(span);
	assert(span->_freeList);
	//将拿到的span里的_freeList里的内存给ThreadCache
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;//因为end实际会少走一步，所以这里初始值为1
	while (i < batchNum - 1 && NextObj(end) != nullptr) {
		end = NextObj(end);
		i++;
		actualNum++;
	}
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_usecount += actualNum;

	_spanLists[index]._mtx.unlock();
	return actualNum;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size) {
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	while (start) {
		void* next = NextObj(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_usecount--;
		if (!span->_usecount) {
			//说明这个时候span就可以再还给page_cache了
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			//释放span给PageCache的时候，使用PageCache的锁就可以了
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();
		
			_spanLists[index]._mtx.lock();
		}
		start = next;
	}

	_spanLists[index]._mtx.unlock();

}

