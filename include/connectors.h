#pragma once

#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
using namespace std;

template <typename T>
struct MultiProducerMultiConsumerQueue 
{
  MultiProducerMultiConsumerQueue();

  void stop();

  bool empty();

  bool push(const T& x);

  bool pop(T& x);

  mutex m;
  condition_variable cv;
  queue<T> q;
  bool isShutdown;
};


template <typename T, int SIZE=(1<<16)>
struct SingleProducerSingleConsumerQueue 
{
  SingleProducerSingleConsumerQueue(); 

  bool isLockFree();

  void forcePush(const T& x);

  bool push(const T& x);

  bool pop(T& x);

  atomic<size_t> head, tail;
  T buffer[SIZE];
};



template <typename T, int SIZE>
SingleProducerSingleConsumerQueue<T,SIZE>::SingleProducerSingleConsumerQueue() : head(0), tail(0) {} 

template <typename T, int SIZE>
bool SingleProducerSingleConsumerQueue<T,SIZE>::isLockFree() 
{
  return head.is_lock_free() && tail.is_lock_free();
}

template <typename T, int SIZE>
void SingleProducerSingleConsumerQueue<T,SIZE>::forcePush(const T& x) 
{
  while (false == push(x))
  {
    this_thread::yield(); 
  }
}

template <typename T, int SIZE>
bool SingleProducerSingleConsumerQueue<T,SIZE>::push(const T& x) 
{
  size_t current_head = head.load(memory_order_relaxed);
  if (SIZE == (current_head - tail.load(memory_order_acquire)))
  {
    return false;
  }

  buffer[current_head % SIZE] = x;
  head.store(current_head+1, memory_order_release);
  return true;
}

template <typename T, int SIZE>
bool SingleProducerSingleConsumerQueue<T,SIZE>::pop(T& x) 
{

  size_t current_tail = tail.load(memory_order_relaxed);
  if (current_tail == head.load(memory_order_acquire)) 
  {
    return false;
  }

  x = buffer[current_tail % SIZE];
  tail.store(current_tail+1, memory_order_release);
  return true;
}

template <typename T>
MultiProducerMultiConsumerQueue<T>::MultiProducerMultiConsumerQueue() : isShutdown(false) {} 

template <typename T>
bool MultiProducerMultiConsumerQueue<T>::empty() 
{
  return q.empty();
}

template <typename T>
void MultiProducerMultiConsumerQueue<T>::stop() 
{
  isShutdown = true;
  cv.notify_all();
}

template <typename T>
bool MultiProducerMultiConsumerQueue<T>::push(const T& x) 
{
  unique_lock<mutex> lm(m);
  q.push(x);
  cv.notify_one();
  return true;
}

template <typename T>
bool MultiProducerMultiConsumerQueue<T>::pop(T& x)
{
  unique_lock<mutex> lm(m);
  cv.wait(lm, [&](){ return (false == q.empty()) || (true == isShutdown); });

  if (true == isShutdown) return false;

  x = q.front();
  q.pop();
  return true;
}

