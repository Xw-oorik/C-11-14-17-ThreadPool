#include"pool.h"

class MyTask :public Task {
public:
	MyTask(int start,int end ):start_(start),end_(end){}
	Any run() { //run���վ����̳߳ط�����߳��и�������

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
    //�˳�resultҲҪ������linux���ܻ�������vs�µ��������������������ײ����ͷ���Դ����linux�µ�condû���ͷ���Դ��ʲô��û��
    std::cout << "main over" << std::endl;
   // getchar();
#else
    {

        Pool pool;
        //�û��Լ������̳߳صĹ���ģʽ
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
