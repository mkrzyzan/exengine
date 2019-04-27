#include <iostream>
#include <unordered_map>
#include <deque>
#include <queue>
#include <thread>
#include <atomic>
#include <utility>
#include <cassert>
#include <chrono>
#include <mutex>
#include <condition_variable>


//tests
#include "gtest/gtest.h"
using namespace std;



//======================= MULTITHREADING PIPES =======================
template <typename T>
struct MultiProducerMultiConsumerQueue {
  mutex m;
  condition_variable cv;
  queue<T> q;
  bool isShutdown;

  MultiProducerMultiConsumerQueue();

  bool push(T x);

  bool pop(T& x);
};


template <typename T, int SIZE>
struct SingleProducerSingleConsumerQueue {
  atomic<size_t> head, tail;
  T buffer[SIZE];

  SingleProducerSingleConsumerQueue(); 

  bool isLockFree();

  bool push(T x);

  bool pop(T& x);
};


template <typename T, int SIZE>
SingleProducerSingleConsumerQueue<T,SIZE>::SingleProducerSingleConsumerQueue() : head(0), tail(0) {} 

template <typename T, int SIZE>
bool SingleProducerSingleConsumerQueue<T,SIZE>::isLockFree() {
  return head.is_lock_free() && tail.is_lock_free();
}

template <typename T, int SIZE>
bool SingleProducerSingleConsumerQueue<T,SIZE>::push(T x) {
  size_t current_head = head.load(memory_order_relaxed);
  if (SIZE-1 == (current_head - tail.load(memory_order_acquire))) return false;

  buffer[current_head % SIZE] = x;
  head.store(current_head+1, memory_order_release);
  return true;
}

template <typename T, int SIZE>
bool SingleProducerSingleConsumerQueue<T,SIZE>::pop(T& x) {
  size_t current_tail = tail.load(memory_order_relaxed);
  if (current_tail == head.load(memory_order_acquire)) return false;

  x = buffer[current_tail % SIZE];
  tail.store(current_tail+1, memory_order_release);
  return true;
}

template <typename T>
MultiProducerMultiConsumerQueue<T>::MultiProducerMultiConsumerQueue() : isShutdown(false) {} 

template <typename T>
bool MultiProducerMultiConsumerQueue<T>::push(T x) {
  unique_lock<mutex> lm(m);
  q.push(x);
  cv.notify_one();
  return true;
}

template <typename T>
bool MultiProducerMultiConsumerQueue<T>::pop(T& x) {
  unique_lock<mutex> lm(m);
  cv.wait(lm, [&](){ return (false == q.empty()) || (true == isShutdown); });

  if (true == isShutdown) return false;

  x = q.front();
  q.pop();
  return true;
}




//======================= MATCHING ENGINE ============================
enum Side {Buy, Sell, None};
enum EventType {OrderPlaced, Exec, Tick};

struct InputOrder {
  char instrument;
  uint16_t trader;
  uint16_t qty;
  Side side;
  bool operator==(const InputOrder& rhs) { return instrument == rhs.instrument&& trader == rhs.trader && qty == rhs.qty && side == rhs.side; }
};

struct Order {
  uint16_t trader;
  uint16_t qty;
  uint16_t remainQty;
  Side side;
};

struct Event {
  EventType type;
  char instrument;
  uint16_t trader;
  uint16_t qty;
  Side side;

  bool operator==(const Event& rhs) { return type == rhs.type && instrument == rhs.instrument&& trader == rhs.trader && qty == rhs.qty && side == rhs.side; }
};

struct Notifier {
  Notifier();

  SingleProducerSingleConsumerQueue<Event, 512> events;
  unordered_map<uint16_t, SingleProducerSingleConsumerQueue<Event, 512>*> clients;
  bool isShutdown;

  void registerClient(uint16_t id, SingleProducerSingleConsumerQueue<Event, 512>* events);

  void run();
};



Notifier::Notifier() : isShutdown(false) {}

void Notifier::run() {
  while (false == isShutdown) {
    Event event;
    if (true == events.pop(event))
    {
      switch(event.type)
      {
        case EventType::Exec:
        case EventType::OrderPlaced:
        {
          clients[event.trader]->push(event);
          break;
        }
        case EventType::Tick:
        {
          // publishing MarketData (TODO: needed?)
          break;
        }
      }

      // logging events to file right here...
    }
    else
    {
      this_thread::yield(); //not needed if busy loop
    }
  }
}







struct Book {
  Book() : actualSide(None), outstandingQty(0) {}
  uint16_t outstandingQty;
  Side actualSide;
  deque<Order> orders;
};

struct Engine {
  Engine(Notifier& notifier);

  Notifier& notify;
  unordered_map<char, Book> books;
  MultiProducerMultiConsumerQueue<InputOrder> q;
  bool isShutdown;

  void run();
  
  void placeOrder(char instrument, Side side, uint16_t trader, uint16_t qty);
};



Engine::Engine(Notifier& notifier) : books(), notify(notifier), isShutdown(false) {
  books['A'] = {};
  books['B'] = {};
  books['C'] = {};
}

void Engine::run() {
  while (false == isShutdown)
  {
    // blocking call
    InputOrder newOrder;
    q.pop(newOrder); 
    placeOrder(newOrder.instrument, newOrder.side, newOrder.trader, newOrder.qty);
  }
}

void Engine::placeOrder(char instrument, Side side, uint16_t trader, uint16_t qty) {
  Book& book = books[instrument];
  uint16_t remainQty = qty;


  if (true == book.orders.empty() || side == book.actualSide) {
    book.actualSide = side;
    book.orders.push_back(Order{trader, qty, qty, side});
    book.outstandingQty += qty;

    notify.events.push({OrderPlaced, instrument, trader, qty, side});
  }
  else
  {
    while (false == book.orders.empty() && 0 != remainQty) {
      Order& top = book.orders.front();
      if (top.remainQty > remainQty)
      {
        top.remainQty -= remainQty;
        book.outstandingQty -= remainQty;
        remainQty = 0;
      }
      else {
        remainQty -= top.remainQty;
        book.orders.pop_front();
        book.outstandingQty -= top.remainQty;

        notify.events.push({Exec, instrument, top.trader, top.qty, top.side});
      }
    }

    if (0 == remainQty)
    {
      notify.events.push({Exec, instrument, trader, qty, side});
    }
    else
    {
      book.actualSide = side;
      book.orders.push_back(Order{trader, qty, remainQty, side});
      book.outstandingQty += remainQty;
      notify.events.push({OrderPlaced, instrument, trader, qty, side});
    }
  }

  // market data
  if (false == book.orders.empty())
  {
    notify.events.push({Tick, instrument, 0, book.outstandingQty, book.actualSide});
  }
  else
  {
    notify.events.push({Tick, instrument, 0, 0, None});
  }
}





struct TradingTool;
struct Exchange {
  Exchange() : engine(notif) {}

  Notifier notif;
  Engine engine;

  void registerClient(uint16_t id, TradingTool* client);
};

struct TradingTool {
  TradingTool(uint16_t identifier);

  SingleProducerSingleConsumerQueue<Event, 512> events;
  MultiProducerMultiConsumerQueue<InputOrder>* q;
  uint16_t id;
  bool isShutdown;

  void connectTo(Exchange& ex);

  void run(); 
};



TradingTool::TradingTool(uint16_t identifier) : id(identifier), isShutdown(false) {}

void TradingTool::connectTo(Exchange& ex) {
  q = &ex.engine.q; 
  ex.registerClient(id, this);
}

void TradingTool::run() {
  while (false == isShutdown)
  {
    Event event;
    if (true == events.pop(event))
    {
      switch(event.type)
      {
        case EventType::Exec:
        {
          // handle exec
          break;
        }
        case EventType::OrderPlaced:
        {
          // handle orderPlaced
          break;
        }
      }
    }
    else
    {
      this_thread::yield(); // not needed if spin lock
    }
  }
}


void Exchange::registerClient(uint16_t id, TradingTool* client) {
  notif.registerClient(id, &client->events);
}

void Notifier::registerClient(uint16_t id, SingleProducerSingleConsumerQueue<Event, 512>* events) {
  clients[id] = events;
}



//========================  TESTS TESTS ==============================
TEST(MatchingEngineTest, FourOrders)
{
  Exchange ex;
  Notifier& notif = ex.notif;
  Engine& eng = ex.engine;
  Event event;

  eng.placeOrder('A', Buy, 666, 100);
  eng.placeOrder('A', Buy, 777, 200);
  eng.placeOrder('A', Sell, 888, 200);
  eng.placeOrder('A', Sell, 888, 100);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'A',666,100,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,100,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'A',777,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,300,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',666,100,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',888,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,100,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',777,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',888,100,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,0,None}) == event);
  ASSERT_TRUE (false == notif.events.pop(event));
}

TEST(MatchingEngineTest, CasesFromEmail)
{
  Exchange ex;
  Notifier& notif = ex.notif;
  Engine& eng = ex.engine;
  Event event;

  eng.placeOrder('S', Buy, 1, 200);
  eng.placeOrder('S', Sell, 2, 200);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',1,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',1,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',2,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,0,None}) == event);

  eng.placeOrder('G', Sell, 3, 300);
  eng.placeOrder('G', Buy, 4, 200);
  eng.placeOrder('G', Buy, 5, 200);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'G',3,300,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'G',0,300,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'G',4,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'G',0,100,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'G',3,300,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'G',5,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'G',0,100,Buy}) == event);

  eng.placeOrder('H', Sell, 6, 200);
  eng.placeOrder('H', Sell, 7, 200);
  eng.placeOrder('H', Sell, 8, 200);
  eng.placeOrder('H', Buy, 9, 600);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'H',6,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'H',7,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,400,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'H',8,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,600,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',6,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',7,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',8,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',9,600,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,0,None}) == event);
  ASSERT_TRUE (false == notif.events.pop(event));
}

TEST(MatchingEngineTest, OverEatingOneSide)
{
  Exchange ex;
  Notifier& notif = ex.notif;
  Engine& eng = ex.engine;
  Event event;

  eng.placeOrder('S', Buy, 1, 100);
  eng.placeOrder('S', Buy, 2, 200);
  eng.placeOrder('S', Buy, 3, 300);
  eng.placeOrder('S', Buy, 4, 400);
  eng.placeOrder('S', Buy, 5, 500);
  eng.placeOrder('S', Sell, 6, 100);
  eng.placeOrder('S', Sell, 7, 100);
  eng.placeOrder('S', Sell, 8, 100);
  eng.placeOrder('S', Sell, 9, 150);
  eng.placeOrder('S', Sell, 10, 300);
  eng.placeOrder('S', Sell, 11, 100);
  eng.placeOrder('S', Sell, 12, 700);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',1,100,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,100,Buy}) == event);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',2,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,300,Buy}) == event);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',3,300,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,600,Buy}) == event);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',4,400,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1000,Buy}) == event);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',5,500,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1500,Buy}) == event);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',1,100,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',6,100,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1400,Buy}) == event);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',7,100,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1300,Buy}) == event);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',2,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',8,100,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1200,Buy}) == event);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',9,150,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1050,Buy}) == event);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',3,300,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',10,300,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,750,Buy}) == event);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',11,100,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,650,Buy}) == event);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',4,400,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',5,500,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',12,700,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,50,Sell}) == event);
  ASSERT_TRUE (false == notif.events.pop(event));

  //assert (true == notif.events.pop(event)); 
  //cout << "type="<<event.type << ", instrument="<<event.instrument << ", trader=" << event.trader <<", qt=" << event.qty<<  endl;;
}

TEST(ThreadSafeQueueTest, OneThread_perf)
{
  MultiProducerMultiConsumerQueue<InputOrder> q; 
  InputOrder order;

  for (uint32_t i = 0; i < 1000000; i++)
  {
    uint16_t x = static_cast<uint16_t>(i);
    q.push(InputOrder{'A', x,x, (x % 2) ? Buy : Sell});

    if (i >= 10000)
    {
      q.pop(order);
      ASSERT_TRUE ((InputOrder{'A', static_cast<uint16_t>(x-10000), static_cast<uint16_t>(x-10000), (x % 2) ? Buy : Sell}) == order);
    }
  }

  for (int i = 0; i < 10000; i++) q.pop(order);
}

TEST(ThreadSafeQueueTest, TwoThreads_perf)
{
  MultiProducerMultiConsumerQueue<InputOrder> q; 
  thread t1([&]() {
    for (uint32_t i = 0; i < 1000000; i++)
    {
      uint16_t x = static_cast<uint16_t>(i);
      q.push(InputOrder{'A', x, x, (x % 2) ? Buy : Sell});
    }
  });

  for (uint32_t i = 0; i < 1000000; i++)
  {
    uint16_t x = static_cast<uint16_t>(i);
    InputOrder order;
    q.pop(order);
    ASSERT_TRUE ((InputOrder{'A', x, x, (x % 2) ? Buy : Sell}) == order);
  }

  t1.join();
}

TEST(LocklessQueueTest, TwoThreads_perf)
{
  SingleProducerSingleConsumerQueue<Event,800> q;
  thread t1([&]() {
    uint32_t c = 0;
    while (c < 1000000)
    {
      uint16_t x = static_cast<uint16_t>(c);
      if (false == q.push(Event{Exec, 'A', x, x, (x % 2) ? Buy : Sell}))
      {
        this_thread::yield();
      }
      else
      {
        c++;
      }
    }
  });

  uint32_t c = 0; 
  while (c < 1000000)
  {
    uint16_t x = static_cast<uint16_t>(c);
    Event event;
    if (false == q.pop(event))
    {
      this_thread::yield();
    }
    else
    {
      ASSERT_TRUE ((Event{Exec, 'A', x, x, (x % 2) ? Buy : Sell}) == event);
      c++;
    }
  }

  t1.join();
}

//========================   MAIN MAIN  ==============================
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
