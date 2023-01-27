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

//Any类型，可以接受任意类型的数据
class Any {
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;//用unique不准左值的拷贝 赋值
    Any& operator=(const Any&) = delete;
    Any( Any&&) = default;//右值的可以
    Any& operator=(Any&&) = default;

    //这个构造函数可以让Any类型接收任意其他类型的数据
    template<typename T>
    Any(T data) :base_(std::make_unique<Derive<T>>(data)) {}

    //这个方法能把Any对象里面存储的任意类型数据data提取出来
    template<typename T>
    T cast_() {
        //我们怎么从base里面找到他所指向的派生类derive对象，出去data成员变量
        Derive<T>* ptr = dynamic_cast<Derive<T>*>(base_.get());
        if (ptr == nullptr) {
            throw" type is unmatch!";
        }
        return ptr->data_;
    }

private:
    //基类类型
    class Base {
    public:
        virtual ~Base() = default;
    };
    //派生类类型
    template<typename T>
    class Derive :public Base {
    public:
        Derive(T data):data_(data){}

        T data_;//在派生类保存着返回的任意类型的值
    };
private:
    //定义一个基类指针 指向派生类对象
    std::unique_ptr<Base> base_;

};

//自己实现一个信号量类
class Semaphore {
private:
    int resource_;
    std::mutex mtx_;
    std::condition_variable cond_;
    std::atomic_bool isExit_;//为了在解决linux下，result都析构了还要，调用底层条件变量啥的，条件变量没有析构的问题
public:
    Semaphore(int resource=0)
        :resource_(resource)
        ,isExit_(false)
    {}
    ~Semaphore()
    {
        isExit_ = true;
    }
    //获取一个信号量资源 -1
    void wait() {
        if (isExit_)return;
        std::unique_lock<std::mutex> lock(mtx_);
        //有资源了-1 没有阻塞
        cond_.wait(lock, [&]()->bool {return resource_ > 0; });
        resource_--;
    }
    //增加一个信号量资源  +1
    void post() {
        if (isExit_)return;
        std::unique_lock<std::mutex> lock(mtx_);
        resource_++;
        //一个问题？？
        //调用的都是默认的析构，linux下condition_variable析构什么事情都没做
        //没有释放资源，导致这里状态失效，无故阻塞
        cond_.notify_all();
    }
};

class Task;//声明
//实现接收提交到线程池的task任务执行之后返回的结果类型
class Result {
private:
    Any any_;//储存任务的返回值
    Semaphore sem_;
    std::shared_ptr<Task>task_;//指向对应获取返回值的任务对象
    std::atomic_bool isValid_;//返回值是否有效
public:
    Result(std::shared_ptr<Task> task, bool isValid=true);
    ~Result() = default;
    void setVal(Any any);//获取线程 run任务执行完的返回值

    Any get();//用户调用这个方法获取result返回值
};

//任务task抽象基类
class Task {
private:
    Result* result_;
public:
    Task();
    ~Task() = default;
    void setResult(Result* res);
    void exec();
    //用户继承task类，自定义实现处理任务的接口
    virtual Any run() = 0;
};
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
    void startThread();//启动线程 
    Thread(ThreadFunc fun);
    ~Thread();
    int getId()const;//获取线程id

private:
    ThreadFunc func;
    int threadId_;//保存线程i
    static int generateId_;
    std::condition_variable exitCond_;
};

/*
example:

class MyTask:public Task{
public:
    void run(){...线程代码}
};
Pool pool
pool.SetMode(**);
pool.startPool();
pool.submitTask(std::make_shared<MyTask>());

*/
//线程池类
class Pool {
private:
    //std::vector<std::unique_ptr<Thread>>threads;//线程列表
    std::unordered_map<int,std::unique_ptr<Thread>>threads;//线程队列
    size_t initThreadSizes;//初始的线程个数

    std::queue<std::shared_ptr<Task>> taskQue;//任务队列 
    std::atomic_uint tasksize;//记录任务个数
    size_t taskQueMaxThreads;//任务队列的任务数量上限
    size_t threadMaxSize_;//cached模式 线程数量上限阈值

    std:: mutex taskQueMutex;//保证任务队列安全
    std::condition_variable notFull;//不满
    std::condition_variable notEmpty;//不空

    PoolMODE poolmode;//当前线程模式
    std::atomic_bool isPoolRunning_;//当前线程池的设置状态
    std::atomic_uint ideThreadSize_;//记录空闲线程的数量
    std::atomic_uint curThreadSize_; //cached模式 记录当前线程池里线程的总数
    std::condition_variable exitCond_;//等待线程资源全部回收
private:
    void threadFunc(int threadid);//定义线程工作函数
    bool checkPoolState()const;//检查pool的运行状态
public:
    Pool();
    ~Pool();
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    //void setInitThreadSize(size_t size);//提供最初的线程数量
    void startPool(size_t initThreadSize = std::thread::hardware_concurrency());//开启线程池 默认根据核心数初始化
    void SetMode(PoolMODE mode);//开启线程模式
    void setTaskMaxQueHold(int taskQueMaxHoldThreads);//设置认任务队列任务上限的阈值
    void setThreadMaxhold(int threadMaxSizehold);//设置cached模式线程上限

    Result submitTask(std::shared_ptr<Task> sk);//给线程池里任务队列提交任务
};

#endif
