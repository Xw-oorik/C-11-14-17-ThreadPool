#ifndef POOL_H
#define POOL_H

#include<iostream>
#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<thread>
#include<unordered_map>

//Any���ͣ����Խ����������͵�����
class Any {
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;//��unique��׼��ֵ�Ŀ��� ��ֵ
    Any& operator=(const Any&) = delete;
    Any( Any&&) = default;//��ֵ�Ŀ���
    Any& operator=(Any&&) = default;

    //������캯��������Any���ͽ��������������͵�����
    template<typename T>
    Any(T data) :base_(std::make_unique<Derive<T>>(data)) {}

    //��������ܰ�Any��������洢��������������data��ȡ����
    template<typename T>
    T cast_() {
        //������ô��base�����ҵ�����ָ���������derive���󣬳�ȥdata��Ա����
        Derive<T>* ptr = dynamic_cast<Derive<T>*>(base_.get());
        if (ptr == nullptr) {
            throw" type is unmatch!";
        }
        return ptr->data_;
    }

private:
    //��������
    class Base {
    public:
        virtual ~Base() = default;
    };
    //����������
    template<typename T>
    class Derive :public Base {
    public:
        Derive(T data):data_(data){}

        T data_;//�������ౣ���ŷ��ص��������͵�ֵ
    };
private:
    //����һ������ָ�� ָ�����������
    std::unique_ptr<Base> base_;

};

//�Լ�ʵ��һ���ź�����
class Semaphore {
private:
    int resource_;
    std::mutex mtx_;
    std::condition_variable cond_;
    std::atomic_bool isExit_;//Ϊ���ڽ��linux�£�result�������˻�Ҫ�����õײ���������ɶ�ģ���������û������������
public:
    Semaphore(int resource=0)
        :resource_(resource)
        ,isExit_(false)
    {}
    ~Semaphore()
    {
        isExit_ = true;
    }
    //��ȡһ���ź�����Դ -1
    void wait() {
        if (isExit_)return;
        std::unique_lock<std::mutex> lock(mtx_);
        //����Դ��-1 û������
        cond_.wait(lock, [&]()->bool {return resource_ > 0; });
        resource_--;
    }
    //����һ���ź�����Դ  +1
    void post() {
        if (isExit_)return;
        std::unique_lock<std::mutex> lock(mtx_);
        resource_++;
        //һ�����⣿��
        //���õĶ���Ĭ�ϵ�������linux��condition_variable����ʲô���鶼û��
        //û���ͷ���Դ����������״̬ʧЧ���޹�����
        cond_.notify_all();
    }
};

class Task;//����
//ʵ�ֽ����ύ���̳߳ص�task����ִ��֮�󷵻صĽ������
class Result {
private:
    Any any_;//��������ķ���ֵ
    Semaphore sem_;
    std::shared_ptr<Task>task_;//ָ���Ӧ��ȡ����ֵ���������
    std::atomic_bool isValid_;//����ֵ�Ƿ���Ч
public:
    Result(std::shared_ptr<Task> task, bool isValid=true);
    ~Result() = default;
    void setVal(Any any);//��ȡ�߳� run����ִ����ķ���ֵ

    Any get();//�û��������������ȡresult����ֵ
};

//����task�������
class Task {
private:
    Result* result_;
public:
    Task();
    ~Task() = default;
    void setResult(Result* res);
    void exec();
    //�û��̳�task�࣬�Զ���ʵ�ִ�������Ľӿ�
    virtual Any run() = 0;
};
//����ģʽ
enum class PoolMODE {
    MODE_FIXED,//�̶��������߳�
    MODE_CACHED//�߳��������Զ�̬����
};


/////////////////////////////////////////////////////////
//�߳���
class Thread {

public:
    using ThreadFunc = std::function<void(int)>;
    void startThread();//�����߳� 
    Thread(ThreadFunc fun);
    ~Thread();
    int getId()const;//��ȡ�߳�id

private:
    ThreadFunc func;
    int threadId_;//�����߳�i
    static int generateId_;
    std::condition_variable exitCond_;
};

/*
example:

class MyTask:public Task{
public:
    void run(){...�̴߳���}
};
Pool pool
pool.SetMode(**);
pool.startPool();
pool.submitTask(std::make_shared<MyTask>());

*/
//�̳߳���
class Pool {
private:
    //std::vector<std::unique_ptr<Thread>>threads;//�߳��б�
    std::unordered_map<int,std::unique_ptr<Thread>>threads;//�̶߳���
    size_t initThreadSizes;//��ʼ���̸߳���

    std::queue<std::shared_ptr<Task>> taskQue;//������� 
    std::atomic_uint tasksize;//��¼�������
    size_t taskQueMaxThreads;//������е�������������
    size_t threadMaxSize_;//cachedģʽ �߳�����������ֵ

    std:: mutex taskQueMutex;//��֤������а�ȫ
    std::condition_variable notFull;//����
    std::condition_variable notEmpty;//����

    PoolMODE poolmode;//��ǰ�߳�ģʽ
    std::atomic_bool isPoolRunning_;//��ǰ�̳߳ص�����״̬
    std::atomic_uint ideThreadSize_;//��¼�����̵߳�����
    std::atomic_uint curThreadSize_; //cachedģʽ ��¼��ǰ�̳߳����̵߳�����
    std::condition_variable exitCond_;//�ȴ��߳���Դȫ������
private:
    void threadFunc(int threadid);//�����̹߳�������
    bool checkPoolState()const;//���pool������״̬
public:
    Pool();
    ~Pool();
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    //void setInitThreadSize(size_t size);//�ṩ������߳�����
    void startPool(size_t initThreadSize = std::thread::hardware_concurrency());//�����̳߳� Ĭ�ϸ��ݺ�������ʼ��
    void SetMode(PoolMODE mode);//�����߳�ģʽ
    void setTaskMaxQueHold(int taskQueMaxHoldThreads);//��������������������޵���ֵ
    void setThreadMaxhold(int threadMaxSizehold);//����cachedģʽ�߳�����

    Result submitTask(std::shared_ptr<Task> sk);//���̳߳�����������ύ����
};

#endif
