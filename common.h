#pragma once
#include<iostream>
#include<algorithm>
#include<assert.h>
#include<time.h>
#include<thread>
#include <atomic>
#include<mutex>
#include"ConCurrent.h"
#include<vector>
#include<unordered_map>
#include<string.h>

using std::cin;
using std::cout;
using std::endl;
#ifdef _WIN32
	#include<windows.h>
#else
	//...
#endif


#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#endif

inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage * (1 << 12), MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}




inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}

static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREELIST = 208;
static const size_t NPAGES = 129;
static const size_t PAGE_SHIFT = 13;

static void*& NextObj(void* obj) {
	return *(void**)obj;
}

//管理切分好的小的对象的自由链表
class FreeList {
public:
	void Push(void* obj) {
		//头插
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}

	void PushRange(void*& start, void*& end,size_t n) {
		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}
	void PopRange(void*& start, void*& end, size_t n) {
		assert(n <= _size);
		start = _freeList;
		end = start;
		for (size_t i = 0; i < n - 1; i++) {
			end = NextObj(end);
			//if(NextObj(end) == nullptr)
			//cout << end << " " << NextObj(end) << endl;
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}

	void* Pop() {
		//头删
		assert(_freeList);
		void* obj = _freeList;
		_freeList = NextObj(obj);
		NextObj(obj) = nullptr;
		--_size;
		return obj;
	}
	bool empty() {
		return _size == 0;
	}

	size_t& Maxsize() {
		return _maxSize;
	}
	size_t size() {
		return _size;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0;
};


//计算对象大小的对其映射规则
class SizeClass {
public:
	//映射的规则可以有很多，tcmalloc实际的实现方式更加复杂，我们这里就简化一下，按照下面的规则来实现对其
	// 整体控制在最多10%左右的内碎片浪费
 // [1,128]                8byte对齐        freelist[0,16)  16个桶
 // [128+1,1024]           16byte对齐       freelist[16,72) 56个桶
 // [1024+1,8*1024]        128byte对齐      freelist[72,128)
 // [8*1024+1,64*1024]     1024byte对齐     freelist[128,184)
 // [64*1024+1,256*1024]   8*1024byte对齐   freelist[184,208)

	//也可以这么写
	/*size_t _RoundUp(size_t size, size_t AlignNum) {
		return ((size + AlignNum - 1) & ~(AlignNum - 1));
	}*/

	static inline size_t _RoundUp(size_t size, size_t AlignNum) {//AlignNum表示对齐数
		size_t alignSize;
		if (size % AlignNum != 0) {
			alignSize = (size / AlignNum + 1) * AlignNum;
		}
		else {
			alignSize = size;
		}
		return alignSize;
	}
	
	static inline size_t RoundUp(size_t size) {//作用，算出来其对其之后的数是多少  size = 16
		if (size <= 128) {
			return _RoundUp(size, 8);          //返回16
		}
		else if (size <= 1024) {
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024) {
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024) {
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024) {
			return _RoundUp(size, 8*1024);
		}
		else {
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}

	//需要算一下是在哪一号桶里面

	/*size_t _Index(size_t bytes, size_t alignNum) {
		if (bytes % alignNum == 0) {
			return bytes / alignNum - 1;
		}
		else {
			return bytes / alignNum;
		}
	}*/

	//该函数可以这么来写
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	// 计算映射的哪一个自由链表桶
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128) {
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) {
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if (bytes <= 8 * 1024) {
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 64 * 1024) {
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1]
				+ group_array[0];
		}
		else if (bytes <= 256 * 1024) {
			return _Index(bytes - 64 * 1024, 13) + group_array[3] +
				group_array[2] + group_array[1] + group_array[0];
		}
		else {
			assert(false);
		}
		return -1;
	}

	// 一次thread_cache从中心缓存获取多少个
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		if (size == 0)
			return 0;
		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 小对象一次批量上限低
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)//申请字节数过小导致num过大，让其有个上限
			num = 512;
		return num;
	}
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;  
		npage >>= PAGE_SHIFT;   //字节数转换成page数
		if (npage == 0)
			npage = 1;
		return npage;
	}
};

struct Span {
	PAGE_ID _pageid = 0; //大块内存起始页的页号
	size_t _n = 0;       //页的数量

	Span* _next = nullptr;     //双向链表的前驱
	Span* _prev = nullptr;     //指向双向链表的后继

	size_t _usecount = 0; //切好的小块内存，被分配给thread_cache的计数
	size_t _objSize = 0;//切好的小对象的大小

	void* _freeList = nullptr; //切好的小块内存的自由链表

	bool isUsed = false;
};

//双向带头循环链表
class SpanList {
public:
	SpanList() {
		_head = spanPool.New();
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* begin() {
		return _head->_next;
	}

	Span* end() {
		return _head;
	}

	void PushFront(Span* span) {
		Insert(begin(), span);
	}

	Span* PopFront() {
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	void Insert(Span* pos, Span* newSpan) {
		assert(pos);
		assert(newSpan);

		Span* prev = pos->_prev;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
		prev->_next = newSpan;
	}

	bool Empty() {
		return _head->_next == _head;
	}

	void Erase(Span* pos) {
		assert(pos);
		assert(pos != _head);

		//1、条件断点
		//if (pos != _head) {
		//	int x = 0;//2\然后调用堆栈,查看栈帧
		//}
		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;
	}
private:
	Span* _head;
	ObjectPool<Span> spanPool;
public:
	std::mutex _mtx;//桶锁

};
