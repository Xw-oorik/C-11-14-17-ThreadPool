#include"pool.h"

class MyTask :public Task {
public:
	MyTask(int start,int end ):start_(start),end_(end){}
	Any run() { //run最终就在线程池分配的线程中干事情了

		std::cout << "begin id" << std::this_thread::get_id() << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		unsigned long long sum = 0;
		for (unsigned long long i = start_; i < end_; ++i) {
			sum += i;
		}
		std::cout << "end id" << std::this_thread::get_id() << std::endl;

		return sum;
	}
private:
	int start_;
	int end_;
};
int main()
{  
#if 1
    {
        Pool pool;
        //pool.SetMode(PoolMODE::MODE_CACHED);
        pool.startPool(2);
        Result res = pool.submitTask(std::make_shared<MyTask>(1, 10000));
        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        unsigned long long sum = res.get().cast_<unsigned long long>();
        std::cout << sum << std::endl;
    }
    //退出result也要析构，linux下跑会阻塞，vs下的条件变量的析构函数底层有释放资源，而linux下的cond没有释放资源，什么都没做
    std::cout << "main over" << std::endl;
   // getchar();
#else
    {

        Pool pool;
        //用户自己设置线程池的工作模式
        pool.SetMode(PoolMODE::MODE_CACHED);
        pool.startPool(4);
        Result res = pool.submitTask(std::make_shared<MyTask>(1, 1000000));
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1000001, 2000000));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(2000001, 3000000));
        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        unsigned long long sum = res.get().cast_<unsigned long long>();
        unsigned long long sum1 = res1.get().cast_<unsigned long long>();
        unsigned long long sum2 = res2.get().cast_<unsigned long long>();

        std::cout << sum + sum1 + sum2 << std::endl;
    }
	getchar();


	return 0;
#endif
}
