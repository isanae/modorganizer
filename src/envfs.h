#ifndef ENV_ENVFS_H
#define ENV_ENVFS_H

#include <thread>
#include <condition_variable>
#include <filesystem>
#include <atomic>

namespace env
{

class Waiter
{
public:
  Waiter()
    : m_ready(false)
  {
  }

  void wait()
  {
    std::unique_lock lock(m_mutex);
    m_cv.wait(lock, [&]{ return m_ready; });
    m_ready = false;
  }

  void wakeup()
  {
    {
      std::scoped_lock lock(m_mutex);
      m_ready = true;
    }

    m_cv.notify_one();
  }

private:
  std::condition_variable m_cv;
  std::mutex m_mutex;
  bool m_ready;
};


template <class T>
class ThreadPool
{
public:
  ThreadPool(std::size_t max=1)
  {
    setMax(max);
  }

  ~ThreadPool()
  {
    stopAndJoin();
  }

  void setMax(std::size_t n)
  {
    m_threads.resize(n);
  }

  void stopAndJoin()
  {
    for (auto& ti : m_threads) {
      ti.stop = true;
      ti.wakeup();
    }

    for (auto& ti : m_threads) {
      if (ti.thread.joinable()) {
        ti.thread.join();
      }
    }
  }

  void waitForAll()
  {
    for (;;) {
      bool done = true;

      for (auto& ti : m_threads) {
        if (ti.busy) {
          done = false;
          break;
        }
      }

      if (done) {
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  T& request()
  {
    if (m_threads.empty()) {
      std::terminate();
    }

    for (;;) {
      for (auto& ti : m_threads) {
        bool expected = false;

        if (ti.busy.compare_exchange_strong(expected, true)) {
          ti.wakeup();
          return ti.o;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  template <class F>
  void forEach(F&& f)
  {
    for (auto& ti : m_threads) {
      f(ti.o);
    }
  }

private:
  struct ThreadInfo
  {
    std::thread thread;
    std::atomic<bool> busy;
    T o;

    Waiter waiter;
    std::atomic<bool> stop;

    ThreadInfo()
      : busy(true), stop(false)
    {
      thread = std::thread([&]{ run(); });
    }

    ~ThreadInfo()
    {
      if (thread.joinable()) {
        stop = true;
        wakeup();
        thread.join();
      }
    }

    void wakeup()
    {
      waiter.wakeup();
    }

    void run()
    {
      busy = false;

      while (!stop) {
        waiter.wait();

        if (stop) {
          break;
        }

        o.run();
        busy = false;
      }
    }
  };

  std::list<ThreadInfo> m_threads;
};


using DirStartF = void (void*, std::wstring_view);
using DirEndF = void (void*, std::wstring_view);
using FileF = void (void*, std::wstring_view, std::filesystem::file_time_type);

void setHandleCloserThreadCount(std::size_t n);


class DirectoryWalker
{
public:
  void forEachEntry(
    const std::wstring& path, void* cx,
    DirStartF* dirStartF, DirEndF* dirEndF, FileF* fileF);

private:
  std::vector<std::unique_ptr<unsigned char[]>> m_buffers;
};

} // namespace

#endif // ENV_ENVFS_H
