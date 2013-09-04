#ifndef _SIMPLE_THREAD_H_
#define _SIMPLE_THREAD_H_

namespace base
{

class SimpleThread
{
public:
    SimpleThread();

    virtual ~SimpleThread();

    // 启动线程, 保证Start()函数结束时，线程已经开始执行
    virtual void Start();

    virtual void Stop();

    virtual bool IsStop();

    // 等待线程关闭
    virtual void Join();

    // 子类需要重载此函数
    // Run()函数运行在新建的线程内
    virtual void Run() = 0;

    HANDLE ThreadId() { return thread_id_; }

    void ThreadMain();

private:
    HANDLE thread_id_;
    bool is_stop_;
};

} // namespace Thread

#endif // _SIMPLE_THREAD_H_
