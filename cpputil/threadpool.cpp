#include "threadpool.h"

CThreadPool::~CThreadPool()
{
    Stop();
}

int CThreadPool::Start(int iThreadNum)
{
    if (m_status != EStatus::STOP)
    {
        return DEFAULT_ERROR_CODE;
    }
    m_status = EStatus::RUNNING;

    if (iThreadNum < m_iMinThreadNum)
    {
        iThreadNum = m_iMinThreadNum;
    }

    if (iThreadNum > m_iMaxThreadNum)
    {
        iThreadNum = m_iMaxThreadNum;
    }

    for (int i = 0; i < iThreadNum; ++i)
    {
        CreateThread();
    }
    return DEFAULT_NORMAL_CODE;
}

int CThreadPool::Stop()
{
    if (EStatus::STOP == m_status)
    {
        return DEFAULT_ERROR_CODE;
    }
    m_status = EStatus::STOP;
    m_condThread.notify_all();

    for (auto &td : m_threadList)
    {
        if (td.thread->joinable())
        {
            td.thread->join();
        }
    }
    m_threadList.clear();
    m_iCurThreadNum = 0;
    m_iIdleThreadNum = 0;

    return DEFAULT_NORMAL_CODE;
}

void CThreadPool::DoWork()
{
    while (m_status != EStatus::STOP)
    {
        // 如果线程池状态是暂停的话，让出cpu
        while (EStatus::PAUSE == m_status)
        {
            std::this_thread::yield();
        }

        Task task;
        {
            std::unique_lock<std::mutex> lock(m_mutexThread);
            // 如果任务队列是空或者线程状态是停止的话
            m_condThread.wait_for(lock, std::chrono::milliseconds(m_iMaxIdleTimeout), [this]()
                                  { return m_status == EStatus::STOP || !m_taskQueue.empty(); });

            if (EStatus::STOP == m_status)
            {
                return;
            }

            if (m_taskQueue.empty())
            {
                if (m_iCurThreadNum > m_iMinThreadNum)
                {
                    // 删除多余线程
                    DeleteThread(std::this_thread::get_id());
                    return;
                }
                continue;
            }
            --m_iIdleThreadNum;
            task = std::move(m_taskQueue.front());
            m_taskQueue.pop();
        }

        // 执行任务
        if (task)
        {
            task();
            ++m_iIdleThreadNum;
        }
    }
}

void CThreadPool::DeleteThread(std::thread::id id)
{
    std::unique_lock<std::mutex> lock(m_mutexThread);
    --m_iCurThreadNum;
    --m_iIdleThreadNum;

    // 获取当前时间
    auto endTime = std::chrono::steady_clock::now();

    auto iter = m_threadList.begin();
    while (iter != m_threadList.end())
    {
        // 线程池状态是停止的话，删除所有线程
        if (iter->threadStatus == EStatus::STOP && endTime > iter->stopTime)
        {
            if (iter->thread->joinable())
            {
                iter->thread->join();
                iter = m_threadList.erase(iter);
                continue;
            }
        }
        else if (iter->thread->get_id() == id)
        {
            iter = m_threadList.erase(iter);
        }
        ++iter;
    }
}

bool CThreadPool::CreateThread()
{
    if (m_iCurThreadNum < m_iMaxThreadNum)
    {
        std::thread *thread = new std::thread(std::bind(&CThreadPool::DoWork, this));

        ++m_iCurThreadNum;
        ++m_iIdleThreadNum;

        // 存储线程信息
        std::unique_lock<std::mutex> lock(m_mutexThread);
        SThreadData sThreadData;
        sThreadData.thread = std::shared_ptr<std::thread>(thread);
        sThreadData.id = thread->get_id();
        sThreadData.threadStatus = EStatus::RUNNING;
        sThreadData.startTime = std::chrono::steady_clock::now();
        //sThreadData.stopTime = 0;
        m_threadList.emplace_back(sThreadData);
    }
    return false;
}
