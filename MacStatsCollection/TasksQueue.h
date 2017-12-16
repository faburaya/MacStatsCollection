#ifndef __TasksQueue_h__ // header guard
#define __TasksQueue_h__

#include "CommonDataExchange.h"
#include <3FD\utils_lockfreequeue.h>
#include <memory>
#include <vector>
#include <mutex>

namespace application
{
    using namespace _3fd;

    /// <summary>
    /// A lock-free queue for tasks to provide efficient concurrent access.
    /// </summary>
    class TasksQueue
    {
    private:

        utils::Win32ApiWrappers::LockFreeQueue<std::unique_ptr<StatsPackage>> m_queue;

        static std::mutex singletonCreationMutex;

        static std::unique_ptr<TasksQueue> singleton;

        TasksQueue() {}

    public:

        static TasksQueue &GetInstance();

        static void Finalize();

        void Enqueue(std::unique_ptr<StatsPackage> &&task);

        void Dequeue(std::vector<std::unique_ptr<StatsPackage>> &tasks);
    };

}// end of namespace application

#endif // header guard
