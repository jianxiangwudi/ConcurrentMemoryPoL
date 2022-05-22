#include"ThreadCache.h"
#include"CentralCache.h"

void* ThreadCache::Allocate(size_t size) {//size = 16
	assert(size <= MAXBYTE);
	size_t alignSize = SizeClass::RoundUp(size);//16
	size_t index = SizeClass::Index(size);
	if (!_freeLists[index].empty()) {
		return _freeLists[index].Pop();//�����Ļ�������������PopΪ�߳�������ڴ��ʱ��Ͳ���Ҫ�پ������ˣ�Ҳ����Ҫ�ٵȴ��ˡ�
	}
	else {//���Ϊ�գ������Ͱ�е���Դ�Ѿ������꣬��ô��ʱ��Ҫ�����Ļ���ȥȥ������
		return FetchFromCentralCache(index, alignSize);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size) {
	assert(ptr);
	assert(size <= MAX_BYTES);

	//�ҵ���Ӧӳ�����������Ͱ��������������
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);
	
	//����һ�λ��������ڴ�Ƚ϶ࣨ����һ������������ô�Ҿ�һ������������Central_cache����ȥ��
	
	//�����ȴ���һ��������ڴ�ʱ�Ϳ�ʼ��һ��list��central cache
	if (_freeLists[index].size() >= _freeLists[index].Maxsize()) {
		ListTooLong(_freeLists[index], size);
	}
	//�����Խ�һ���Ż�����Ͱ��������ڴ泬�� �˶��٣��ͽ����ͷ�
}


void ThreadCache::ListTooLong(FreeList& list , size_t size) {
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.Maxsize());

	CentralCache::GetInstace()->ReleaseListToSpans(start, size);



}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
	//...
	//����ʼ�ĵ����㷨
	//1���ʼ����һ����centralҪ̫�࣬��Ϊһ��Ҫ̫���ò��ꣻ
	//2������㲻����size��С���ڴ�������ôbatchNum�ͻ᲻��������ֱ������
	//3��sizeԽ����һ����central cacheҪ��batchNumҪ�ľͺ���
	//4��sizeԽ����һ����central cacheҪ��batchNumҪ�ľͺܴ���������

	

	//һ��������ȡ������
	size_t batchNum = min(_freeLists[index].Maxsize(), SizeClass::NumMoveSize(size));
	if (batchNum == _freeLists[index].Maxsize()) {
		_freeLists[index].Maxsize()++;
	}
	void* start = nullptr;
	void* end = nullptr;


	size_t actualNum = CentralCache::GetInstace()->FetchRangeObj(start,end,batchNum,size) ;
	assert(actualNum > 0);
	if (actualNum == 1) {
		//_freeLists[index].Push(start);
		//assert(start == end);
		return start;
	}
	else {
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);
		return start;
	}	
}