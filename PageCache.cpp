#include"PageCache.h"

PageCache PageCache::_sInst;
Span* PageCache::NewSpan(size_t k) {
	assert(k > 0);

	//大于128kb的时候直接找堆去要
	if (k > NPAGES - 1) {
		void* ptr = SystemAlloc(k);
		Span* span = spanPool.New();
		span->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT;//定义出pageid是为了在回收的时候定位到相邻的页
		span->_n = k;//这里的k就表示需要申请多大的页

		//[span->_pageid] = span;
		_idSpanMap.set(span->_pageid, span);

		return span;
	}
	
	//先去查看对应的第k号桶里面有没有对应的span
	
	//先检查第k个桶里面有没有span(注意此时我们已经是在PageCache的桶里面了)
	if (!_spanLists[k].Empty()) {
		Span* kSpan =  _spanLists[k].PopFront();
		for (PAGE_ID i = 0; i < kSpan->_n; i++) {
			_idSpanMap.set(kSpan->_pageid + i, kSpan);
			//_idSpanMap[kSpan->_pageid + i] = kSpan;
		}
		return kSpan;
	}
	//检查一下后面的桶里面有没有span
	for (size_t i = k + 1; i < NPAGES; i++) {
		if (!_spanLists[i].Empty()) {
			//开始切
			//切分一个k页的span和一个n-k页的span，分别放到对应的桶下
			
			//从原本是n页的span上切k页（span）下来
			Span* nSpan = _spanLists[i].PopFront();//从该桶上取出一个span(这里是nspan，意为后面有span的地方)
			Span* kSpan = spanPool.New();

			kSpan->_pageid = nSpan->_pageid;//页号
			kSpan->_n = k;   //kSpan的页数

			nSpan->_pageid += k;//nSpan剩余页号
			nSpan->_n -= k;  //被切走了k页，剩下n-k页
			//存储n-k  span的首位页号和nSPan进行映射，

			//我们这里只要映射首和尾就可以了，前面的页号来和它的首去合并；后面的页号和它的尾去合并
			_idSpanMap.set(nSpan->_pageid,nSpan);
			_idSpanMap.set(nSpan->_pageid + nSpan->_n - 1, nSpan);


			//将nSpan挂到对应的位置
			_spanLists[nSpan->_n].PushFront(nSpan);

			//建立id和span的映射关系，方便central cache回收小块内存时，查找对应的span
			for (PAGE_ID i = 0; i < kSpan->_n; i++) {
				_idSpanMap.set(kSpan->_pageid + i, kSpan);
			}

			return kSpan;//kSpan返回
		}
	}

	//走到这个位置就说明没有大页的span了
	//这个时候就是找堆要一个128页的span
	Span* bigSpan = spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;
	_spanLists[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj) {
	PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);
	/*std::unique_lock<std::mutex> lock(_pageMtx);

	auto ret = _idSpanMap.find(id);
	if (ret != _idSpanMap.end()) {
		return ret->second;
	}
	else {
		assert(false);
		return nullptr;
	}*/
	auto ret = (Span*)_idSpanMap.get(id);
	assert(ret != nullptr);
	return ret;
}


void  PageCache::ReleaseSpanToPageCache(Span* span) {
	
	//大于128kb的时候直接还给堆
	if (span->_n > NPAGES - 1) {
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		SystemFree(ptr);
		spanPool.Delete(span);
		return;
	}
	//为了解决内存碎片的问题，我们需要将其进行合并(即对span前后的页进行合并)
	//通过_idSpanMap来进行前后合并的尝试

	//所有在PageCache里面的内存页都可以去合并
	
	//向前合并

	

	while (1) {
		PAGE_ID prevId = span->_pageid - 1;
		//auto ret = _idSpanMap.find(prevId);
		////前面相邻页没有，不合并
		//if (ret == _idSpanMap.end()) {
		//	break;
		//}
		auto ret = (Span*)_idSpanMap.get(prevId);
		if (ret == nullptr)break;
		Span* prevSpan = ret;
		//前面页相邻页的span在使用，不合并
		if (prevSpan->isUsed) {
			break;
		}
		if (prevSpan->_n + span->_n > NPAGES - 1) {
			break;
		}

		span->_pageid = prevSpan->_pageid;
		span->_n += prevSpan->_n;

		_spanLists[prevSpan->_n].Erase(prevSpan);
		//delete prevSpan;
		spanPool.Delete(prevSpan);
	}

	//向后合并
	while (1) {
		PAGE_ID nextid = span->_pageid + span->_n;
		auto ret = (Span*)_idSpanMap.get(nextid);
		if (ret == nullptr) {
			break;
		}
		Span* nextSpan = ret;
		if (nextSpan->isUsed) { break; }
		if (nextSpan->_n + span->_n > NPAGES - 1) { break; }
		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);
		//delete nextSpan;
		spanPool.Delete(nextSpan);

	}
	_spanLists[span->_n].PushFront(span);
	span->isUsed = false;
	_idSpanMap.set(span->_pageid, span);
	_idSpanMap.set(span->_pageid + span->_n - 1, span);
}