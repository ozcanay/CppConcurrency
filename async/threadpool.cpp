template <typename T>
class ThreadPool {
public:
  using task_type = std::packaged_task<T()>;

  std::future<T> enqueue(task_type task) {
      // could be passed by reference as well...
      // ...or implemented with perfect forwarding
    std::future<T> res = task.get_future();
    { std::lock_guard<std::mutex> lock(mutex_);
      tasks_.push(std::move(task));
    }
    cv_.notify_one();
    return res;
  }

  void worker() { 
    while (true) {  // supposed to be run forever for simplicity
      task_type task;
      { std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]{ return !this->tasks_.empty(); });
        task = std::move(tasks_.top());
        tasks_.pop();
      }
      task();
    }
  }
  ... // constructors, destructor,...
private:
  std::vector<std::thread> workers_;
  std::queue<task_type> tasks_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

https://stackoverflow.com/questions/18143661/what-is-the-difference-between-packaged-task-and-async?rq=1


https://github.com/progschj/ThreadPool --> !!!!!!

https://github.com/bshoshany/thread-pool

https://github.com/lzpong/threadpool/blob/master/threadpool.h

https://github.com/mtrebi/thread-pool