#pragma once
#include"common.h"

//��Ϊ�������е��̹߳���ģ�������Ҫ���Ϊ����ģʽ
//�������Ϊ����ģʽ
class CentralCache {
public:
	static CentralCache* GetInstace() {
		return &_sInst;
	}

	//��ȡһ���ǿյ�span
	Span* GetOneSpan(SpanList& list, size_t byte_size);
	//�����Ļ����л�ȡһ�������Ķ����thread_cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t byte_size);

	//����ȥ�Ҷ�Ӧ��span
	void ReleaseListToSpans(void* start, size_t size);
private:
	CentralCache()
	{}

	CentralCache(const CentralCache&) = delete;
	SpanList _spanLists[NFREELIST];
	static CentralCache _sInst;
};