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
using namespace std;



//======================= MULTITHREADING PIPES =======================
template <typename T, int SIZE>
struct SingleProdSingleConsQueue {
  atomic<size_t> head, tail;
  T buffer[SIZE];

  SingleProdSingleConsQueue (); 

  bool isLockFree();

  bool push(T x);

  bool pop(T& x);
};


template <typename T, int SIZE>
SingleProdSingleConsQueue<T,SIZE>::SingleProdSingleConsQueue () : head(0), tail(0) {} 

template <typename T, int SIZE>
bool SingleProdSingleConsQueue<T,SIZE>::isLockFree() {
  return head.is_lock_free() && tail.is_lock_free();
}

template <typename T, int SIZE>
bool SingleProdSingleConsQueue<T,SIZE>::push(T x) {
  size_t current_head = head.load(memory_order_relaxed);
  if (SIZE-1 == (current_head - tail.load(memory_order_acquire))) return false;

  buffer[current_head % SIZE] = x;
  head.store(current_head+1, memory_order_release);
  return true;
}

template <typename T, int SIZE>
bool SingleProdSingleConsQueue<T,SIZE>::pop(T& x) {
  size_t current_tail = tail.load(memory_order_relaxed);
  if (current_tail == head.load(memory_order_acquire)) return false;

  x = buffer[current_tail % SIZE];
  tail.store(current_tail+1, memory_order_release);
  return true;
}


template <typename T>
struct MultiProducerMultiConsumerQueue {
  mutex m;
  condition_variable cv;
  queue<T> q;

  void push(T x);

  T pop();
};


template <typename T>
void MultiProducerMultiConsumerQueue<T>::push(T x) {
  unique_lock<mutex> lm(m);
  q.push(x);
  cv.notify_one();
}

template <typename T>
T MultiProducerMultiConsumerQueue<T>::pop() {
  unique_lock<mutex> lm(m);
  cv.wait(lm, [&](){ return false == q.empty(); });

  T x = q.front();
  q.pop();
  return x;
}




//======================= MATCHING ENGINE ============================
enum Side {Buy, Sell, None};
enum EventType {OrderPlaced, Exec, Tick};

struct InputOrder {
  char instrument;
  uint16_t trader;
  uint16_t qty;
  Side side;
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

  SingleProdSingleConsQueue<Event, 512> events;
  unordered_map<uint16_t, SingleProdSingleConsQueue<Event, 512>*> clients;
  bool isShutdown;

  void registerClient(uint16_t id, SingleProdSingleConsQueue<Event, 512>* events);

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
  Book() : actualSide(None) {}
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
    InputOrder newOrder = q.pop(); 
    placeOrder(newOrder.instrument, newOrder.side, newOrder.trader, newOrder.qty);
  }
}

void Engine::placeOrder(char instrument, Side side, uint16_t trader, uint16_t qty) {
  Book& book = books[instrument];
  uint16_t remainQty = qty;


  if (true == book.orders.empty() || side == book.actualSide) {
    book.actualSide = side;
    book.orders.push_back(Order{trader, qty, qty, side});

    notify.events.push({OrderPlaced, instrument, trader, qty, side});
  }
  else
  {
    while (false == book.orders.empty() && 0 != remainQty) {
      Order& top = book.orders.front();
      if (top.remainQty > remainQty)
      {
        top.remainQty -= remainQty;
        remainQty = 0;
      }
      else {
        remainQty -= top.remainQty;
        book.orders.pop_front();

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
      notify.events.push({OrderPlaced, instrument, trader, qty, side});
    }
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

  SingleProdSingleConsQueue<Event, 512> events;
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

void Notifier::registerClient(uint16_t id, SingleProdSingleConsQueue<Event, 512>* events) {
  clients[id] = events;
}


//========================   MAIN MAIN  ==============================
int main() {
  cout << "start\n";
  Event event;

  Exchange ex;
  TradingTool t1(666); t1.connectTo(ex); 
  TradingTool t2(777); t2.connectTo(ex); 




  
  Notifier notif;
  Engine eng(notif);

  eng.placeOrder('A', Buy, 666, 100);
  eng.placeOrder('A', Buy, 777, 200);
  eng.placeOrder('A', Sell, 888, 200);
  eng.placeOrder('A', Sell, 888, 100);
  assert (true == notif.events.pop(event) && (Event{OrderPlaced,'A',666,100,Buy}) == event);
  assert (true == notif.events.pop(event) && (Event{OrderPlaced,'A',777,200,Buy}) == event);
  assert (true == notif.events.pop(event) && (Event{Exec,'A',666,100,Buy}) == event);
  assert (true == notif.events.pop(event) && (Event{Exec,'A',888,200,Sell}) == event);
  assert (true == notif.events.pop(event) && (Event{Exec,'A',777,200,Buy}) == event);
  assert (true == notif.events.pop(event) && (Event{Exec,'A',888,100,Sell}) == event);
  assert (false == notif.events.pop(event)); 
  eng.placeOrder('A', Sell, 12, 10);
  eng.placeOrder('A', Sell, 13, 20);
  eng.placeOrder('A', Sell, 14, 30);
  eng.placeOrder('A', Buy, 15, 10);
  eng.placeOrder('A', Buy, 16, 20);
  eng.placeOrder('A', Buy, 17, 30);
  eng.placeOrder('A', Buy, 18, 40);
  assert (true == notif.events.pop(event) && (Event{OrderPlaced,'A',12,10,Sell}) == event);
  assert (true == notif.events.pop(event) && (Event{OrderPlaced,'A',13,20,Sell}) == event);
  assert (true == notif.events.pop(event) && (Event{OrderPlaced,'A',14,30,Sell}) == event);
  assert (true == notif.events.pop(event) && (Event{Exec,'A',12,10,Sell}) == event);
  assert (true == notif.events.pop(event) && (Event{Exec,'A',15,10,Buy}) == event);
  assert (true == notif.events.pop(event) && (Event{Exec,'A',13,20,Sell}) == event);
  assert (true == notif.events.pop(event) && (Event{Exec,'A',16,20,Buy}) == event);
  assert (true == notif.events.pop(event) && (Event{Exec,'A',14,30,Sell}) == event);
  assert (true == notif.events.pop(event) && (Event{Exec,'A',17,30,Buy}) == event);
  assert (true == notif.events.pop(event) && (Event{OrderPlaced,'A',18,40,Buy}) == event);
  assert (false == notif.events.pop(event)); 

 //assert (true == notif.events.pop(event)); 
  //cout << "type="<<event.type << ", instrument="<<event.instrument << ", trader=" << event.trader <<", qt=" << event.qty<<  endl;;
 

  //assert (true == notif.events.pop(event) && (Event{Exec,'A',888,0,Sell}) == event);


 //assert (true == notif.events.pop(event)); 
  //cout << "type="<<event.type << ", instrument="<<event.instrument << ", trader=" << event.trader <<", qt=" << event.qty<<  endl;;






  return 0;
}
