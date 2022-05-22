#include"common.h"
#include"CentralCache.h"
#include"PageCache.h"

CentralCache CentralCache::_sInst;

Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size) {

	//�鿴��ǰ��spanlist���Ƿ���δ����������span
	Span* it = list.begin();
	while (it != list.end()) {
		if (it->_freeList != nullptr) {
			return it;
		}
		else {
			it = it->_next;
		}
	}

	//�Ȱ�central cache��Ͱ����������Ļ�����������߳����ͷŶ���������Ͳ�������
	list._mtx.unlock();	
	//�ߵ������˵��û�п��е�span�ˣ���Ҫ��page_spanҪ
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(byte_size));
	span->isUsed = true;
	span->_objSize = byte_size;
	PageCache::GetInstance()->_pageMtx.unlock();

	//��span�����зֵ�ʱ��Ͳ���Ҫ��������Ϊ���ʱ�������̷߳��ʲ������span


	//�ù�������ô��span��ͨ��NewSpan�õ���һ����Ҫ��������з֡�
	//����ͨ��ҳ�ţ�����ҳ����ʼ��ַ���ڴ�Ĵ�С�����ֽ�Ϊ��λ��
	char* start = (char*)(span->_pageid << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;

	//����Ѵ����ڴ��г�����������������������е���Thread_cache����
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
	//���ｫ�кõ�span�ҵ�Ͱ�����ʱ���ټ���
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
	//���õ���span���_freeList����ڴ��ThreadCache
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;//��Ϊendʵ�ʻ�����һ�������������ʼֵΪ1
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
			//˵�����ʱ��span�Ϳ����ٻ���page_cache��
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			//�ͷ�span��PageCache��ʱ��ʹ��PageCache�����Ϳ�����
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

