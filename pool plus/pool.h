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
#include<chrono>
#include<future>
const int TASK_QUE_MAX_THREADS = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 60; // ��λ����
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
    void startThread()//�����߳� 
    {
        //����һ���߳���ִ���̺߳���
        std::thread t(func, threadId_);
        t.detach();//���÷��� �̺߳����Լ�ȥִ���Լ�������
    }
    Thread(ThreadFunc fun) :func(fun)
        , threadId_(generateId_++)
    {}
    ~Thread() = default;
    int getId()const//��ȡ�߳�id
    {
        return threadId_;
    }

private:
    ThreadFunc func;
    int threadId_;//�����߳�i
    static int generateId_;
    std::condition_variable exitCond_;
};

//�̳߳���
class Pool {
private:
    //std::vector<std::unique_ptr<Thread>>threads;//�߳��б�
    std::unordered_map<int, std::unique_ptr<Thread>>threads;//�̶߳���
    size_t initThreadSizes;//��ʼ���̸߳���
    
    using Task = std::function<void()>;//��������
    std::queue<Task> taskQue;//������� 

    std::atomic_uint tasksize;//��¼�������
    size_t taskQueMaxThreads;//������е�������������
    size_t threadMaxSize_;//cachedģʽ �߳�����������ֵ

    std::mutex taskQueMutex;//��֤������а�ȫ
    std::condition_variable notFull;//����
    std::condition_variable notEmpty;//����

    PoolMODE poolmode;//��ǰ�߳�ģʽ
    std::atomic_bool isPoolRunning_;//��ǰ�̳߳ص�����״̬
    std::atomic_uint ideThreadSize_;//��¼�����̵߳�����
    std::atomic_uint curThreadSize_; //cachedģʽ ��¼��ǰ�̳߳����̵߳�����
    std::condition_variable exitCond_;//�ȴ��߳���Դȫ������
private:
    void threadFunc(int threadid)//�����̹߳�������
    {
        auto lastTime = std::chrono::high_resolution_clock().now();
        //�����������ȫ��ִ����ɣ��̳߳زſ��Ի��������߳���Դ
        while (1)
        {
            //std::shared_ptr<Task>task;
            Task task;
            //��ȡ��
            std::unique_lock<std::mutex> lock(taskQueMutex);

            std::cout << "id" << std::this_thread::get_id() << "���Ի�ȡ����.." << std::endl;

            //��cachedģʽ�£��п����Ѿ������˺ܶ��̣߳�Ҫ�ѿ��еĶ����̻߳��յ�
            //��ǰʱ�䣬��һ���߳�ִ�е�ʱ��
            // ÿһ���з���һ��   ��ô���֣���ʱ���أ������������ִ�з���
            // �� + ˫���ж�
            while (taskQue.size() == 0)
            {
                // �̳߳�Ҫ�����������߳���Դ
                if (!isPoolRunning_)
                {
                    threads.erase(threadid);
                    std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
                        << std::endl;
                    exitCond_.notify_all();
                    return; // �̺߳����������߳̽���
                }

                if (poolmode == PoolMODE::MODE_CACHED)
                {
                    // ������������ʱ������
                    if (std::cv_status::timeout ==
                        notEmpty.wait_for(lock, std::chrono::seconds(1)))
                    {
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if (dur.count() >= THREAD_MAX_IDLE_TIME
                            && curThreadSize_ > initThreadSizes)
                        {
                            // ��ʼ���յ�ǰ�߳�
                            // ��¼�߳���������ر�����ֵ�޸�
                            // ���̶߳�����߳��б�������ɾ��   û�а취 ƥ��threadFunc��=��thread����
                            // threadid => thread���� => ɾ��
                            threads.erase(threadid); // std::this_thread::getid()
                            curThreadSize_--;
                            ideThreadSize_--;

                            std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
                                << std::endl;
                            return;
                        }
                    }
                }
                else
                {
                    // �ȴ�notEmpty����
                    notEmpty.wait(lock);
                }
            }
            //���� ȡ����
            //notEmpty.wait(lock, [&]() {return taskQue.size() > 0; });
            ideThreadSize_--;//�õ�һ���߳� �����߳�--
            std::cout << "id" << std::this_thread::get_id() << "��ȡ����ɹ�.." << std::endl;

            //���������ȡ����
            task = taskQue.front();
            taskQue.pop();
            tasksize--;
            //�����Ȼ��ʣ�����񣬼���֪ͨ�����߳�ȥ������
            if (taskQue.size() > 0) {
                notEmpty.notify_all();
            }
            //ȡ������Ҫ�����ͷŵ�����������һ���߳��õ�����֮�������ͷţ�����߳�ȥ������
            lock.unlock();
            //ȡ������֮�� ֪ͨ�����߿�����������
            notFull.notify_all();

            //ִ������
            if (task != nullptr) {
                //task->run();
                //ִ�������񣬻�ȡ����ֵsetval��result�������û���get��
                //task->exec();//��run��װһ��
                task();
            }
            ideThreadSize_++;//ִ���������ˣ��߳̾Ϳ����� 
            lastTime = std::chrono::high_resolution_clock().now();// �����߳�ִ���������ʱ��
        }
    }
    bool checkPoolState()const//���pool������״̬
    {
        return isPoolRunning_;
    }
public:
    Pool() :initThreadSizes(0)
        , tasksize(0)
        , taskQueMaxThreads(TASK_QUE_MAX_THREADS)
        , poolmode(PoolMODE::MODE_FIXED)
        , isPoolRunning_(false)
        , ideThreadSize_(0)
        , threadMaxSize_(THREAD_MAX_THRESHHOLD)
        , curThreadSize_(0)
    {}
    ~Pool() {
        isPoolRunning_ = false;

        // �ȴ��̳߳��������е��̷߳���  ������״̬������ & ����ִ��������
        std::unique_lock<std::mutex> lock(taskQueMutex);
        notEmpty.notify_all();
        exitCond_.wait(lock, [&]()->bool {return threads.size() == 0; });
    }
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    //void setInitThreadSize(size_t size);//�ṩ������߳�����
    void startPool(size_t initThreadSize = std::thread::hardware_concurrency())//�����̳߳� Ĭ�ϸ��ݺ�������ʼ��
    {
        isPoolRunning_ = true;//�����̳߳�����״̬
        //��ʼ���̸߳���
        initThreadSizes = initThreadSize;
        curThreadSize_ = initThreadSize;
        //�����̶߳���
        for (size_t i = 0; i < initThreadSizes; ++i)
        {
            //����thread��ʱ��Ҫ���̺߳��������̶߳���
            auto ptr = std::make_unique<Thread>(std::bind(&Pool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();

            threads.emplace(threadId, std::move(ptr));//unique��ֵ�Ŀ����͸�ֵ��ɾ�ˣ�����Ļ����ɻ᲻�ɹ�
                                             //ת����ֵ���ô���ȥ����
        }
        //�����߳� 
        for (size_t i = 0; i < initThreadSizes; ++i)
        {
            threads[i]->startThread();//ִ���̳߳�����̵߳ĺ���
            ideThreadSize_++;//��¼��ʼ�����̵߳�����
        }
    }
    void SetMode(PoolMODE mode)//�����߳�ģʽ
    {
            if (checkPoolState()) {//�����false�˾Ͳ��ܿ�����
                return;
            }
            poolmode = mode;
    }
    void setTaskMaxQueHold(int taskQueMaxHoldThreads)//��������������������޵���ֵ
    {
        if (checkPoolState())
            return;
        taskQueMaxThreads = taskQueMaxHoldThreads;
    }

    void setThreadMaxhold(int threadMaxSizehold)//����cachedģʽ�߳�����
    {
        if (checkPoolState())
            return;
        if (poolmode == PoolMODE::MODE_CACHED) {
            threadMaxSize_ = THREAD_MAX_THRESHHOLD;
        }
    }

    //auto submitTask(std::shared_ptr<Task> sk);//���̳߳�����������ύ����
    template<typename Func, typename ... Args>
    auto submitTask(Func&& func, Args&&... args)->std::future<decltype(func(args...))>
    {
        //������񣬷��������������
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
            );
        std::future<RType>result = task->get_future();

        std::unique_lock<std::mutex> lock(taskQueMutex);
        //�û��ύ�����������������1s�������ж�ʧ�ܣ�����

        if (!notFull.wait_for(lock, std::chrono::milliseconds(500)
            , [&]() {return taskQue.size() < taskQueMaxThreads; }))
        {
            //��ʾ����һ��ʱ���ˣ�������Ȼû�����㣬����ʧ��
            std::cerr << "task queue is full submitTask fail." << std::endl;
            auto task = std::make_shared<std::packaged_task<RType()>>(
                []()->RType {return RType(); });
            (*task)();
            return task->get_future();
        }

        //�п��࣬��������������  
        //ȥִ�����������
        taskQue.emplace([task]() {
            (*task)();
            });
        tasksize++;
        //��Ϊ��������������п϶����գ���notEmpty��֪ͨ
        notEmpty.notify_all();

        //cachedģʽ ��Ҫ�������������Ϳ����̵߳��������ж��Ƿ�Ҫ�����µ��̳߳���
        if (poolmode == PoolMODE::MODE_CACHED
            && tasksize > ideThreadSize_
            && curThreadSize_ < threadMaxSize_
            ) {
            std::cout << ">>> create new thread..." << std::endl;
            //�������߳�
            auto ptr = std::make_unique<Thread>(std::bind(&Pool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();

            threads.emplace(threadId, std::move(ptr));//unique��ֵ�Ŀ����͸�ֵ��ɾ�ˣ�����Ļ����ɻ᲻�ɹ�
                                             //ת����ֵ���ô���ȥ����
            // �����߳�
            threads[threadId]->startThread();
            // �޸��̸߳�����صı���
            ideThreadSize_++;
            curThreadSize_++;
        }

        //����result����
        return result;

    }

};


//ʹ�ÿɱ��ģ���̣���submit���Խ���������������������������
int Thread::generateId_ = 0;



#endif
