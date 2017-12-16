#include "stdafx.h"
#include "TasksQueue.h"
#include <3FD\callstacktracer.h>
#include <3FD\exceptions.h>

namespace application
{
    using namespace _3fd::core;


    std::mutex TasksQueue::singletonCreationMutex;

    std::unique_ptr<TasksQueue> TasksQueue::singleton;


    /// <summary>
    /// Provides access to this singleton.
    /// </summary>
    /// <returns>A reference to this singleton.</returns>
    TasksQueue & TasksQueue::GetInstance()
    {
        if (singleton)
            return *singleton;
        
        CALL_STACK_TRACE;

        try
        {
            std::lock_guard<std::mutex> lock(singletonCreationMutex);

            singleton.reset(new TasksQueue());

            return *singleton;
        }
        catch (IAppException &)
        {
            throw; // just forward already prepared application exceptions
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when instantiating tasks queue: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Finalizes the singleton.
    /// </summary>
    void TasksQueue::Finalize()
    {
        singleton.reset(nullptr);
    }


    /// <summary>
    /// Enqueues the specified task.
    /// </summary>
    /// <param name="task">The task.</param>
    void TasksQueue::Enqueue(std::unique_ptr<StatsPackage> &&task)
    {
        CALL_STACK_TRACE;
        m_queue.Push(std::move(task));
    }

    /// <summary>
    /// Dequeues all tasks.
    /// </summary>
    /// <param name="tasks">Will be cleared to receive the dequeued tasks.</param>
    void TasksQueue::Dequeue(std::vector<std::unique_ptr<StatsPackage>> &tasks)
    {
        CALL_STACK_TRACE;

        try
        {
            tasks.clear();

            m_queue.ForEach([&tasks](std::unique_ptr<StatsPackage> &entry)
            {
                tasks.emplace_back(entry.release());
            });
        }
        catch (IAppException &)
        {
            throw; // just forward already prepared application exceptions
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when dequeuing tasks: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

}// end of namespace application
