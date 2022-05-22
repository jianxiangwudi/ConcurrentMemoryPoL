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
	// linux��brk mmap��
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
	// sbrk unmmap��
#endif
}

static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREELIST = 208;
static const size_t NPAGES = 129;
static const size_t PAGE_SHIFT = 13;

static void*& NextObj(void* obj) {
	return *(void**)obj;
}

//�����зֺõ�С�Ķ������������
class FreeList {
public:
	void Push(void* obj) {
		//ͷ��
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
		//ͷɾ
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


//��������С�Ķ���ӳ�����
class SizeClass {
public:
	//ӳ��Ĺ�������кܶ࣬tcmallocʵ�ʵ�ʵ�ַ�ʽ���Ӹ��ӣ���������ͼ�һ�£���������Ĺ�����ʵ�ֶ���
	// ������������10%���ҵ�����Ƭ�˷�
 // [1,128]                8byte����        freelist[0,16)  16��Ͱ
 // [128+1,1024]           16byte����       freelist[16,72) 56��Ͱ
 // [1024+1,8*1024]        128byte����      freelist[72,128)
 // [8*1024+1,64*1024]     1024byte����     freelist[128,184)
 // [64*1024+1,256*1024]   8*1024byte����   freelist[184,208)

	//Ҳ������ôд
	/*size_t _RoundUp(size_t size, size_t AlignNum) {
		return ((size + AlignNum - 1) & ~(AlignNum - 1));
	}*/

	static inline size_t _RoundUp(size_t size, size_t AlignNum) {//AlignNum��ʾ������
		size_t alignSize;
		if (size % AlignNum != 0) {
			alignSize = (size / AlignNum + 1) * AlignNum;
		}
		else {
			alignSize = size;
		}
		return alignSize;
	}
	
	static inline size_t RoundUp(size_t size) {//���ã�����������֮������Ƕ���  size = 16
		if (size <= 128) {
			return _RoundUp(size, 8);          //����16
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

	//��Ҫ��һ��������һ��Ͱ����

	/*size_t _Index(size_t bytes, size_t alignNum) {
		if (bytes % alignNum == 0) {
			return bytes / alignNum - 1;
		}
		else {
			return bytes / alignNum;
		}
	}*/

	//�ú���������ô��д
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	// ����ӳ�����һ����������Ͱ
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		// ÿ�������ж��ٸ���
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

	// һ��thread_cache�����Ļ����ȡ���ٸ�
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		if (size == 0)
			return 0;
		// [2, 512]��һ�������ƶ����ٸ������(������)����ֵ
		// С����һ���������޸�
		// С����һ���������޵�
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)//�����ֽ�����С����num���������и�����
			num = 512;
		return num;
	}
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;  
		npage >>= PAGE_SHIFT;   //�ֽ���ת����page��
		if (npage == 0)
			npage = 1;
		return npage;
	}
};

struct Span {
	PAGE_ID _pageid = 0; //����ڴ���ʼҳ��ҳ��
	size_t _n = 0;       //ҳ������

	Span* _next = nullptr;     //˫�������ǰ��
	Span* _prev = nullptr;     //ָ��˫������ĺ��

	size_t _usecount = 0; //�кõ�С���ڴ棬�������thread_cache�ļ���
	size_t _objSize = 0;//�кõ�С����Ĵ�С

	void* _freeList = nullptr; //�кõ�С���ڴ����������

	bool isUsed = false;
};

//˫���ͷѭ������
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

		//1�������ϵ�
		//if (pos != _head) {
		//	int x = 0;//2\Ȼ����ö�ջ,�鿴ջ֡
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
	std::mutex _mtx;//Ͱ��

};
