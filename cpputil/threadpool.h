/**
 * @file threadpool.h
 * @author wangdong (wangdong1328@163.com)
 * @brief 线程池
 * @version 0.1
 * @date 2023-04-04
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <utility>
#include <chrono>
#include <list>
#include <atomic>
#include <iostream>

#define MIN_THREAD_NUM  1
#define MAX_THREAD_NUM std::thread::hardware_concurrency()
#define MAX_IDLE_TIME 60000

static const int DEFAULT_ERROR_CODE = -1;
static const int DEFAULT_NORMAL_CODE = 0;

class CThreadPool
{
    using Task = std::function<void()>;

public:
    CThreadPool(int iMinThread = MIN_THREAD_NUM,
                int iMaxThread = MAX_THREAD_NUM,
                int iMaxIdleTimeout = MAX_IDLE_TIME) : m_iMinThreadNum(iMinThread),
                                                       m_iMaxThreadNum(iMaxThread),
                                                       m_iMaxIdleTimeout(iMaxIdleTimeout),
                                                       m_status(EStatus::STOP),
                                                       m_iCurThreadNum(0),
                                                       m_iIdleThreadNum(0)
    {
    }

    virtual ~CThreadPool();

    /**
     * @brief 启动线程池
     *
     * @param iThreadNum 线程数量
     * @return int 启动线程池结果
     */
    int Start(int iThreadNum = 0);

    /**
     * @brief 停止线程池
     *
     * @return int 返回值状态
     */
    int Stop();

    /**
     * @brief 线程池
     * 
     * @tparam Fn 
     * @tparam Args 
     * @param fn 
     * @param args 
     * @return std::future<decltype(fn(args...))> 
     */
    template <class Fn, class... Args>
    auto Commit(Fn &&fn, Args &&...args) -> std::future<decltype(fn(args...))>
    {
        if (EStatus::STOP == m_status)
        {
            Start();
        }

        if (m_iIdleThreadNum <= m_taskQueue.size() && m_iCurThreadNum < m_iMaxThreadNum)
        {
            // 创建线程
            CreateThread();
        }

        using RetType = decltype(fn(args...));

        auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));

        std::future<RetType> future = task->get_future();

        {
            std::unique_lock<std::mutex> lock(m_taskQueueMutex);
            m_taskQueue.emplace([task]
                                { (*task)(); });
        }

        m_condThread.notify_one();
        return future;
    }

     /**
     * @brief Get the Cur Thread Num object
     * 
     * @return int 获取当前线程池中线程数量
     */
    int GetCurThreadNum() const { return m_iCurThreadNum; }

    /**
     * @brief Get the Idle Thread Num object
     * 
     * @return int 获取空闲线程数量
     */
    int GetIdleThreadNum() const { return m_iIdleThreadNum; }
    
    /**
     * @brief 暂停线程池
     * 
     * @return int 
     */
    int Pause()
    {
        if (EStatus::RUNNING == m_status)
        {
            m_status = EStatus::PAUSE;
        }
        return 0;
    }

    /**
     * @brief 唤醒线程池
     * 
     * @return int 
     */
    int Resume()
    {
        if (EStatus::PAUSE == m_status)
        {
            m_status = EStatus::RUNNING;
        }
        return 0;
    }

    /**
     * @brief 线程池等待
     * 
     * @return int 
     */
    int Wait()
    {
        while (m_status != EStatus::STOP)
        {
            if (m_taskQueue.empty() && m_iIdleThreadNum == m_iCurThreadNum)
            {
                break;
            }
            std::this_thread::yield();
        }
    }

    /**
     * @brief Get the Task Num object
     * 
     * @return size_t 
     */
    size_t GetTaskNum() 
    {
        std::unique_lock<std::mutex> lock(m_taskQueueMutex);
        return m_taskQueue.size();
    }

    /**
     * @brief Set the Min Thread Num object
     * 
     * @param iMinThreadNum 
     */
    void SetMinThreadNum(int iMinThreadNum)
    {
        m_iMinThreadNum = iMinThreadNum;
    }

    /**
     * @brief Set the Max Thread Num object
     * 
     * @param iMaxThreadNum 
     */
    void SetMaxThreadNum(int iMaxThreadNum)
    {
        m_iMaxThreadNum = iMaxThreadNum;
    }

    /**
     * @brief Set the Max Idle Time object
     * 
     * @param iMaxIdleTime 
     */
    void SetMaxIdleTime(int iMaxIdleTime)
    {
        m_iMaxIdleTimeout = iMaxIdleTime;
    }

    /**
     * @brief 判断线程池是否启动
     * 
     * @return true 
     * @return false 
     */
    bool IsStarted() const
    {
        return m_status != EStatus::STOP;
    }

    /**
     * @brief 判断线程池是否是暂停状态
     * 
     * 
     * @return true 
     * @return false 
     */
    bool IsStopped() const
    {
        return m_status == EStatus::STOP;
    }

protected:
    // 线程池状态
    enum class EStatus : unsigned char
    {
        STOP,
        RUNNING,
        PAUSE,
    };

    // 线程数据
    struct SThreadData
    {
        std::shared_ptr<std::thread> thread;
        std::thread::id id;
        EStatus threadStatus;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point stopTime;
    };

    /**
     * @brief 线程工作函数
     *
     */
    void DoWork();

    /**
     * @brief 根据线程ID删除线程
     *
     * @param id 线程ID
     */
    void DeleteThread(std::thread::id id);

    /**
     * @brief Create a Thread object
     * 
     * @return true 
     * @return false 
     */
    bool CreateThread();

private:
    // 线程池状态
    std::atomic<EStatus> m_status;
    // 线程池锁
    std::mutex m_mutexThread;
    // 线程池条件变量
    std::condition_variable m_condThread;
    // 线程池当前线程数量
    std::atomic<int> m_iCurThreadNum;
    // 线程池空闲线程数量
    std::atomic<int> m_iIdleThreadNum;
    // 线程池线程列表
    std::list<SThreadData> m_threadList;
    // 线程池任务队列
    std::queue<Task> m_taskQueue;
    // 线程池任务队列锁
    std::mutex m_taskQueueMutex;

    int m_iMinThreadNum;
    int m_iMaxThreadNum;
    int m_iMaxIdleTimeout;
};

#endif /* __THREADPOOL_H__ */