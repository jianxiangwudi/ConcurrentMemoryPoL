#include"ConcurrentAlloc.h"
#include"ConCurrent.h"


void Alloc1() {
	std::vector<void*> v;
	for (size_t i = 0; i < 5; i++) {
		void* ptr = ConcurrentAlloc(6);
	}
	for (auto e : v) {
		ConcurrentFree(e);
	}
}
void Alloc2() {
	std::vector<void*> v;
	for (size_t i = 0; i < 5; i++) {
		void* ptr = ConcurrentAlloc(6);
	}
	for (auto e : v) {
		ConcurrentFree(e);
	}
}
void TLSTest() {
	std::thread t1(Alloc1);
	std::thread t2(Alloc2);
	t1.join();
	t2.join();
}

void TestConcurrentAlloc() {
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(7);
	void* p3 = ConcurrentAlloc(8);
	void* p4 = ConcurrentAlloc(9);
	void* p5 = ConcurrentAlloc(1);
	void* p6 = ConcurrentAlloc(10);

	ConcurrentFree(p1);
	ConcurrentFree(p2);
	ConcurrentFree(p3);
	ConcurrentFree(p4);
	ConcurrentFree(p5);
	ConcurrentFree(p6);
}


