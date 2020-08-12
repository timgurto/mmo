#pragma once

#include <functional>
#include <queue>
#include <thread>

// SDL requires all calls to be done in one thread.  This class enables that, by
// providing a task queue.  Any thread can enqueue a task, and a single thread
// will execute each in turn.
class WorkerThread {
 public:
  using Task = std::function<void()>;

  WorkerThread(const std::string &threadName);
  void enqueue(Task task);
  void waitUntilDone();
  void requireThisCallToBeInWorkerThread();

 private:
  void run();
  std::queue<Task> _tasks;
  std::thread::id _workerThreadID;
};
