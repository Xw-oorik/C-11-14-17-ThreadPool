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
const int THREAD_MAX_IDLE_TIME = 60; // 单位：秒
//两种模式
enum class PoolMODE {
    MODE_FIXED,//固定数量的线程
    MODE_CACHED//线程数量可以动态增长
};


/////////////////////////////////////////////////////////
//线程类
class Thread {

public:
    using ThreadFunc = std::function<void(int)>;
    void startThread()//启动线程 
    {
        //创建一个线程来执行线程函数
        std::thread t(func, threadId_);
        t.detach();//设置分离 线程函数自己去执行自己的任务
    }
    Thread(ThreadFunc fun) :func(fun)
        , threadId_(generateId_++)
    {}
    ~Thread() = default;
    int getId()const//获取线程id
    {
        return threadId_;
    }

private:
    ThreadFunc func;
    int threadId_;//保存线程i
    static int generateId_;
    std::condition_variable exitCond_;
};

//线程池类
class Pool {
private:
    //std::vector<std::unique_ptr<Thread>>threads;//线程列表
    std::unordered_map<int, std::unique_ptr<Thread>>threads;//线程队列
    size_t initThreadSizes;//初始的线程个数
    
    using Task = std::function<void()>;//函数对象
    std::queue<Task> taskQue;//任务队列 

    std::atomic_uint tasksize;//记录任务个数
    size_t taskQueMaxThreads;//任务队列的任务数量上限
    size_t threadMaxSize_;//cached模式 线程数量上限阈值

    std::mutex taskQueMutex;//保证任务队列安全
    std::condition_variable notFull;//不满
    std::condition_variable notEmpty;//不空

    PoolMODE poolmode;//当前线程模式
    std::atomic_bool isPoolRunning_;//当前线程池的设置状态
    std::atomic_uint ideThreadSize_;//记录空闲线程的数量
    std::atomic_uint curThreadSize_; //cached模式 记录当前线程池里线程的总数
    std::condition_variable exitCond_;//等待线程资源全部回收
private:
    void threadFunc(int threadid)//定义线程工作函数
    {
        auto lastTime = std::chrono::high_resolution_clock().now();
        //所有任务必须全部执行完成，线程池才可以回收所有线程资源
        while (1)
        {
            //std::shared_ptr<Task>task;
            Task task;
            //获取锁
            std::unique_lock<std::mutex> lock(taskQueMutex);

            std::cout << "id" << std::this_thread::get_id() << "尝试获取任务.." << std::endl;

            //在cached模式下，有可能已经创建了很多线程，要把空闲的多余线程回收掉
            //当前时间，上一次线程执行的时间
            // 每一秒中返回一次   怎么区分：超时返回？还是有任务待执行返回
            // 锁 + 双重判断
            while (taskQue.size() == 0)
            {
                // 线程池要结束，回收线程资源
                if (!isPoolRunning_)
                {
                    threads.erase(threadid);
                    std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
                        << std::endl;
                    exitCond_.notify_all();
                    return; // 线程函数结束，线程结束
                }

                if (poolmode == PoolMODE::MODE_CACHED)
                {
                    // 条件变量，超时返回了
                    if (std::cv_status::timeout ==
                        notEmpty.wait_for(lock, std::chrono::seconds(1)))
                    {
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if (dur.count() >= THREAD_MAX_IDLE_TIME
                            && curThreadSize_ > initThreadSizes)
                        {
                            // 开始回收当前线程
                            // 记录线程数量的相关变量的值修改
                            // 把线程对象从线程列表容器中删除   没有办法 匹配threadFunc《=》thread对象
                            // threadid => thread对象 => 删除
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
                    // 等待notEmpty条件
                    notEmpty.wait(lock);
                }
            }
            //不空 取任务
            //notEmpty.wait(lock, [&]() {return taskQue.size() > 0; });
            ideThreadSize_--;//用掉一个线程 空闲线程--
            std::cout << "id" << std::this_thread::get_id() << "获取任务成功.." << std::endl;

            //从任务队列取任务
            task = taskQue.front();
            taskQue.pop();
            tasksize--;
            //如果依然有剩余任务，继续通知其他线程去拿任务
            if (taskQue.size() > 0) {
                notEmpty.notify_all();
            }
            //取完任务要把锁释放掉，这样才能一个线程拿掉任务之后立马释放，别的线程去拿任务
            lock.unlock();
            //取完任务之后 通知生产者可以生产任务
            notFull.notify_all();

            //执行任务
            if (task != nullptr) {
                //task->run();
                //执行完任务，获取返回值setval给result对象，让用户能get到
                //task->exec();//把run封装一下
                task();
            }
            ideThreadSize_++;//执行任务完了，线程就空闲了 
            lastTime = std::chrono::high_resolution_clock().now();// 更新线程执行完任务的时间
        }
    }
    bool checkPoolState()const//检查pool的运行状态
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

        // 等待线程池里面所有的线程返回  有两种状态：阻塞 & 正在执行任务中
        std::unique_lock<std::mutex> lock(taskQueMutex);
        notEmpty.notify_all();
        exitCond_.wait(lock, [&]()->bool {return threads.size() == 0; });
    }
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    //void setInitThreadSize(size_t size);//提供最初的线程数量
    void startPool(size_t initThreadSize = std::thread::hardware_concurrency())//开启线程池 默认根据核心数初始化
    {
        isPoolRunning_ = true;//设置线程池启动状态
        //初始化线程个数
        initThreadSizes = initThreadSize;
        curThreadSize_ = initThreadSize;
        //创建线程对象
        for (size_t i = 0; i < initThreadSizes; ++i)
        {
            //创建thread的时候要把线程函数给给线程对象
            auto ptr = std::make_unique<Thread>(std::bind(&Pool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();

            threads.emplace(threadId, std::move(ptr));//unique左值的拷贝和赋值被删了，插入的话构成会不成功
                                             //转成右值引用传进去构造
        }
        //启动线程 
        for (size_t i = 0; i < initThreadSizes; ++i)
        {
            threads[i]->startThread();//执行线程池里的线程的函数
            ideThreadSize_++;//记录初始空闲线程的数量
        }
    }
    void SetMode(PoolMODE mode)//开启线程模式
    {
            if (checkPoolState()) {//如果是false了就不能开启了
                return;
            }
            poolmode = mode;
    }
    void setTaskMaxQueHold(int taskQueMaxHoldThreads)//设置认任务队列任务上限的阈值
    {
        if (checkPoolState())
            return;
        taskQueMaxThreads = taskQueMaxHoldThreads;
    }

    void setThreadMaxhold(int threadMaxSizehold)//设置cached模式线程上限
    {
        if (checkPoolState())
            return;
        if (poolmode == PoolMODE::MODE_CACHED) {
            threadMaxSize_ = THREAD_MAX_THRESHHOLD;
        }
    }

    //auto submitTask(std::shared_ptr<Task> sk);//给线程池里任务队列提交任务
    template<typename Func, typename ... Args>
    auto submitTask(Func&& func, Args&&... args)->std::future<decltype(func(args...))>
    {
        //打包任务，放入任务队列里面
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
            );
        std::future<RType>result = task->get_future();

        std::unique_lock<std::mutex> lock(taskQueMutex);
        //用户提交任务，最长不能阻塞超过1s，否则判断失败，返回

        if (!notFull.wait_for(lock, std::chrono::milliseconds(500)
            , [&]() {return taskQue.size() < taskQueMaxThreads; }))
        {
            //表示等了一段时间了，条件依然没有满足，任务失败
            std::cerr << "task queue is full submitTask fail." << std::endl;
            auto task = std::make_shared<std::packaged_task<RType()>>(
                []()->RType {return RType(); });
            (*task)();
            return task->get_future();
        }

        //有空余，把任务放任务队列  
        //去执行下面的任务
        taskQue.emplace([task]() {
            (*task)();
            });
        tasksize++;
        //因为放了任务，任务队列肯定不空，在notEmpty上通知
        notEmpty.notify_all();

        //cached模式 需要根据任务数量和空闲线程的数量，判断是否要创建新的线程出来
        if (poolmode == PoolMODE::MODE_CACHED
            && tasksize > ideThreadSize_
            && curThreadSize_ < threadMaxSize_
            ) {
            std::cout << ">>> create new thread..." << std::endl;
            //创建新线程
            auto ptr = std::make_unique<Thread>(std::bind(&Pool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();

            threads.emplace(threadId, std::move(ptr));//unique左值的拷贝和赋值被删了，插入的话构成会不成功
                                             //转成右值引用传进去构造
            // 启动线程
            threads[threadId]->startThread();
            // 修改线程个数相关的变量
            ideThreadSize_++;
            curThreadSize_++;
        }

        //返回result对象
        return result;

    }

};


//使用可变参模板编程，让submit可以接受任意任务函数和任意数量参数
int Thread::generateId_ = 0;



#endif
