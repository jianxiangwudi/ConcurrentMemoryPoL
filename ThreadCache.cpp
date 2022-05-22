#include"ThreadCache.h"
#include"CentralCache.h"

void* ThreadCache::Allocate(size_t size) {//size = 16
	assert(size <= MAXBYTE);
	size_t alignSize = SizeClass::RoundUp(size);//16
	size_t index = SizeClass::Index(size);
	if (!_freeLists[index].empty()) {
		return _freeLists[index].Pop();//这样的话，我们在这里Pop为线程里分配内存的时候就不需要再竞争锁了，也不需要再等待了。
	}
	else {//如果为空，则表明桶中的资源已经被用完，那么此时就要向中心缓存去去对象了
		return FetchFromCentralCache(index, alignSize);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size) {
	assert(ptr);
	assert(size <= MAX_BYTES);

	//找到对应映射的自由链表桶，将对象插入进入
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);
	
	//假设一次还回来的内存比较多（大于一个批量），那么我就一次性批量地往Central_cache里面去还
	
	//当长度大于一批申请的内存时就开始还一段list给central cache
	if (_freeLists[index].size() >= _freeLists[index].Maxsize()) {
		ListTooLong(_freeLists[index], size);
	}
	//还可以进一步优化，即桶中如果总内存超过 了多少，就进行释放
}


void ThreadCache::ListTooLong(FreeList& list , size_t size) {
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.Maxsize());

	CentralCache::GetInstace()->ReleaseListToSpans(start, size);



}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
	//...
	//慢开始的调节算法
	//1、最开始不会一次向central要太多，因为一次要太多用不完；
	//2、如果你不断有size大小的内存需求，那么batchNum就会不断增长，直到上限
	//3、size越大，那一次向central cache要的batchNum要的就很少
	//4、size越大，那一次向central cache要的batchNum要的就很大（慢增长）

	

	//一次批量获取的数量
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