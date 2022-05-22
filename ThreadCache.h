#pragma once
#include "common.h"




//����֪����ÿһ���߳��ڴ�����ʱ�򣬶������һ��ThreadCache����ô��������ô�������أ�
//����֪����һ�������е�ÿһ���̹߳���һ�ݽ��̵�ַ�ռ䣬���У���һЩ������˽�еģ������ջָ�루ջ������������Ϣ���Ĵ�����������Щ�����ǹ����
//����ȫ�ֱ�������̬������ȫ�������������������ݣ��ȵ�

//����������Ҫ˼�����ǣ�����ô���̺߳�ThreadCache����Ӧ�ø���һ���̴߳����أ�����������봴����ʱ��ȥ������
//������һ���߳��õ�����һ��ThreadCache���ұ�֤�Լ��Ƕ�����صȵ����⡣
class ThreadCache {
public:
	//������ͷſռ�
	void* Allocate(size_t size);

	void Deallocate(void* ptr, size_t size);


	void* FetchFromCentralCache(size_t index, size_t size);

	void ListTooLong(FreeList& list, size_t size);

	
private:
	FreeList _freeLists[NFREELIST];
};


//Ϊʲô����static���Ա�ֻ֤�ڵ�ǰ�ļ��ɼ���
//TLS thread local storage
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;