#pragma once
#include"common.h"
//����ֱ��ȥ��������ռ䣬ͨ��ϵͳ���ýӿ�





//�����ڴ��
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		if (_freeList) {//���Ȱѻ������ķ��ظ��ڴ�����ظ�����
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
		}
		else {
			//ʣ����ڴ治��һ�������Сʱ�����¿����µĿռ䣬����if
			if (_remainBytes < sizeof(T)) {
				_memory = (char*)malloc(128 * 1024); //Ҳ����ֱ����ϵͳȥҪ������malloc��ֱ����ϵͳ���ýӿ�ȥ����
				_remainBytes = 128 * 1024;
				if (!_memory) {
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
		}

		//��λnew����ʾ����T�Ĺ��캯����ʼ��
		new(obj)T;
		return obj;
	}
	void Delete(T* obj) {
		obj->~T();
		//if (_freeList == nullptr) {//���Բ����жϣ����ַ�ʽ��������
		//	_freeList = obj;
		//	*(void**)obj = nullptr;//������ν���ǹؼ���
		//}
		//else {//ͷ��
			*(void**)obj = _freeList;
			_freeList = obj;
		//}
	}
private:
	char* _memory = nullptr;//��char����Ϊ�պ�char��һ���ֽڣ��������зֵ�ʱ����������ƶ���ʱ��ֱ�Ӽ������־Ϳ�����
	//ָ�����ڴ��ָ��

	//�зֹ����е�ʣ���ֽ���
	size_t _remainBytes = 0;
	//���������ôȥ�����أ�
	//������һ����������������
	void* _freeList = nullptr;

};

//template<size_t N>
//class ObjectPool {
//
//};
//�����ڴ��