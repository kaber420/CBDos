#pragma once

#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace rtos {

typedef void* TaskHandle;
typedef void* MutexHandle;
typedef void (*TaskFunction)(void* param);

TaskHandle createTask(TaskFunction fn, const char* name, uint32_t stackSize = 4096, void* param = nullptr, uint32_t priority = 5, int coreId = -1);
void deleteTask(TaskHandle handle = nullptr);
void sleepMs(uint32_t ms);

MutexHandle createMutex();
bool lockMutex(MutexHandle handle, uint32_t timeoutMs = 0xFFFFFFFF);
void unlockMutex(MutexHandle handle);
void deleteMutex(MutexHandle handle);

} // namespace rtos
} // namespace cbdos
