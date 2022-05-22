#include"PageCache.h"

PageCache PageCache::_sInst;
Span* PageCache::NewSpan(size_t k) {
	assert(k > 0);

	//����128kb��ʱ��ֱ���Ҷ�ȥҪ
	if (k > NPAGES - 1) {
		void* ptr = SystemAlloc(k);
		Span* span = spanPool.New();
		span->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT;//�����pageid��Ϊ���ڻ��յ�ʱ��λ�����ڵ�ҳ
		span->_n = k;//�����k�ͱ�ʾ��Ҫ�������ҳ

		//[span->_pageid] = span;
		_idSpanMap.set(span->_pageid, span);

		return span;
	}
	
	//��ȥ�鿴��Ӧ�ĵ�k��Ͱ������û�ж�Ӧ��span
	
	//�ȼ���k��Ͱ������û��span(ע���ʱ�����Ѿ�����PageCache��Ͱ������)
	if (!_spanLists[k].Empty()) {
		Span* kSpan =  _spanLists[k].PopFront();
		for (PAGE_ID i = 0; i < kSpan->_n; i++) {
			_idSpanMap.set(kSpan->_pageid + i, kSpan);
			//_idSpanMap[kSpan->_pageid + i] = kSpan;
		}
		return kSpan;
	}
	//���һ�º����Ͱ������û��span
	for (size_t i = k + 1; i < NPAGES; i++) {
		if (!_spanLists[i].Empty()) {
			//��ʼ��
			//�з�һ��kҳ��span��һ��n-kҳ��span���ֱ�ŵ���Ӧ��Ͱ��
			
			//��ԭ����nҳ��span����kҳ��span������
			Span* nSpan = _spanLists[i].PopFront();//�Ӹ�Ͱ��ȡ��һ��span(������nspan����Ϊ������span�ĵط�)
			Span* kSpan = spanPool.New();

			kSpan->_pageid = nSpan->_pageid;//ҳ��
			kSpan->_n = k;   //kSpan��ҳ��

			nSpan->_pageid += k;//nSpanʣ��ҳ��
			nSpan->_n -= k;  //��������kҳ��ʣ��n-kҳ
			//�洢n-k  span����λҳ�ź�nSPan����ӳ�䣬

			//��������ֻҪӳ���׺�β�Ϳ����ˣ�ǰ���ҳ������������ȥ�ϲ��������ҳ�ź�����βȥ�ϲ�
			_idSpanMap.set(nSpan->_pageid,nSpan);
			_idSpanMap.set(nSpan->_pageid + nSpan->_n - 1, nSpan);


			//��nSpan�ҵ���Ӧ��λ��
			_spanLists[nSpan->_n].PushFront(nSpan);

			//����id��span��ӳ���ϵ������central cache����С���ڴ�ʱ�����Ҷ�Ӧ��span
			for (PAGE_ID i = 0; i < kSpan->_n; i++) {
				_idSpanMap.set(kSpan->_pageid + i, kSpan);
			}

			return kSpan;//kSpan����
		}
	}

	//�ߵ����λ�þ�˵��û�д�ҳ��span��
	//���ʱ������Ҷ�Ҫһ��128ҳ��span
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
	
	//����128kb��ʱ��ֱ�ӻ�����
	if (span->_n > NPAGES - 1) {
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		SystemFree(ptr);
		spanPool.Delete(span);
		return;
	}
	//Ϊ�˽���ڴ���Ƭ�����⣬������Ҫ������кϲ�(����spanǰ���ҳ���кϲ�)
	//ͨ��_idSpanMap������ǰ��ϲ��ĳ���

	//������PageCache������ڴ�ҳ������ȥ�ϲ�
	
	//��ǰ�ϲ�

	

	while (1) {
		PAGE_ID prevId = span->_pageid - 1;
		//auto ret = _idSpanMap.find(prevId);
		////ǰ������ҳû�У����ϲ�
		//if (ret == _idSpanMap.end()) {
		//	break;
		//}
		auto ret = (Span*)_idSpanMap.get(prevId);
		if (ret == nullptr)break;
		Span* prevSpan = ret;
		//ǰ��ҳ����ҳ��span��ʹ�ã����ϲ�
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

	//���ϲ�
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