// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"
#include "task.h"

using namespace xbox::httpclient;

bool HC_CALLING_CONV
HCTaskIsTaskPending()
{
    verify_http_singleton();
    auto& map = get_http_singleton()->m_taskPendingQueue;
    return !map.empty();
}

HC_API void HC_CALLING_CONV
HCTaskSetCompleted(
    _In_ HC_TASK_HANDLE taskHandle
    )
{
    verify_http_singleton();
    http_task_queue_completed(taskHandle);
}

HC_API bool HC_CALLING_CONV
HCTaskIsCompleted(
    _In_ HC_TASK_HANDLE taskHandleId
    )
{
    HC_TASK* taskHandle = http_task_get_task_from_handle_id(taskHandleId);
    if (taskHandle == nullptr)
        return true;

    return taskHandle->state == http_task_state::completed;
}

void HC_CALLING_CONV
HCTaskProcessNextCompletedTask(_In_ uint64_t taskGroupId)
{
    verify_http_singleton();
    HC_TASK* task = http_task_get_next_completed(taskGroupId);
    if (task == nullptr)
        return;

#if ENABLE_LOGS
    LOGS_INFO << L"HCTaskProcessNextCompletedTask: taskGroupId=" << taskGroupId << " taskId=" << task->id;
#endif
    http_task_process_completed(task);

    http_task_clear_task_from_handle_id(task->id);
}

#if UWP_API
HC_API bool HC_CALLING_CONV
HCTaskWaitForCompleted(
    _In_ HC_TASK_HANDLE taskHandleId,
    _In_ uint32_t timeoutInMilliseconds
)
{
    HC_TASK* taskHandle = http_task_get_task_from_handle_id(taskHandleId);
    if (taskHandle == nullptr)
        return true; // already completed

    DWORD dwResult = WaitForSingleObject(taskHandle->resultsReady.get(), timeoutInMilliseconds);
    return (dwResult == WAIT_OBJECT_0);
}


HANDLE HC_CALLING_CONV
HCTaskGetPendingHandle()
{
    verify_http_singleton();
    return get_http_singleton()->get_pending_ready_handle();
}

HANDLE HC_CALLING_CONV
HCTaskGetCompletedHandle(_In_ uint64_t taskGroupId)
{
    verify_http_singleton();
    return get_http_singleton()->get_task_completed_queue_for_taskgroup(taskGroupId)->get_complete_ready_handle();
}
#endif

void HC_CALLING_CONV
HCTaskProcessNextPendingTask()
{
    verify_http_singleton();
    HC_TASK* task = http_task_get_next_pending();
    if (task == nullptr)
        return;

    http_task_process_pending(task);
}

HC_API HC_TASK_HANDLE HC_CALLING_CONV
HCTaskCreate(
    _In_ uint64_t taskGroupId,
    _In_ HC_TASK_EXECUTE_FUNC executionRoutine,
    _In_opt_ void* executionRoutineContext,
    _In_ HC_TASK_WRITE_RESULTS_FUNC writeResultsRoutine,
    _In_opt_ void* writeResultsRoutineContext,
    _In_opt_ void* completionRoutine,
    _In_opt_ void* completionRoutineContext,
    _In_ bool executeNow
    )
{
    verify_http_singleton();

    HC_TASK* pTask = nullptr;

    {
        std::unique_ptr<HC_TASK> task = std::make_unique<HC_TASK>();
        pTask = task.get();
#if UWP_API
        task->resultsReady.set(CreateEvent(NULL, FALSE, FALSE, NULL));
#endif
        task->executionRoutine = executionRoutine;
        task->executionRoutineContext = executionRoutineContext;
        task->writeResultsRoutine = writeResultsRoutine;
        task->writeResultsRoutineContext = writeResultsRoutineContext;
        task->completionRoutine = completionRoutine;
        task->completionRoutineContext = completionRoutineContext;
        task->taskGroupId = taskGroupId;
        task->id = get_http_singleton()->m_lastId++;
#if ENABLE_LOGS
        LOGS_INFO << L"HCTaskCreate: taskGroupId=" << taskGroupId << " taskId=" << task->id;
#endif

        http_task_store_task_from_handle_id(std::move(task));
    }


    if (executeNow)
    {
        http_task_process_pending(pTask);
    }
    else
    {
        http_task_queue_pending(pTask);
    }

    return pTask->id;
}

