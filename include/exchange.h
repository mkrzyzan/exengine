#pragma once

#include <unordered_map>
#include <deque>
#include <thread>
#include <utility>

#include <connectors.h>

using namespace std;

enum Side {Buy, Sell, None};
enum EventType {OrderPlaced, Exec, Tick};

struct InputOrder {
  char instrument;
  uint16_t trader;
  uint16_t qty;
  Side side;
  bool operator==(const InputOrder& rhs)
  { 
    return instrument == rhs.instrument &&
           trader == rhs.trader &&
           qty == rhs.qty && 
           side == rhs.side;
  }
};

struct Order {
  Order(uint16_t trd, uint16_t qt, uint16_t rmQt, Side s) : trader(trd), qty(qt), remainQty(rmQt), side(s) {}
  uint16_t trader;
  uint16_t qty;
  uint16_t remainQty;
  Side side;
};

struct Event {
  EventType type;
  char instrument;
  uint16_t trader;
  uint32_t qty;
  Side side;

  bool operator==(const Event& rhs)
  { 
    return type == rhs.type &&
           instrument == rhs.instrument &&
           trader == rhs.trader &&
           qty == rhs.qty && 
           side == rhs.side;
  }
};

struct Notifier {
  Notifier();

  void registerClient(uint16_t id, SingleProducerSingleConsumerQueue<Event>* events);

  void start();

  void stop();

  void run();

  SingleProducerSingleConsumerQueue<Event> events;
  unordered_map<uint16_t, SingleProducerSingleConsumerQueue<Event>*> clients;
  bool isShutdown;
  thread* the;
};

struct Book {
  Book() : actualSide(None), outstandingQty(0) {}

  uint32_t outstandingQty;
  Side actualSide;
  deque<Order> orders;
};

struct Engine {
  Engine(Notifier& notifier);

  void placeOrder(char instrument, Side side, uint16_t trader, uint16_t qty);

  void run();

  void start();

  void stop();

  Notifier& notify;
  unordered_map<char, Book> books;
  MultiProducerMultiConsumerQueue<InputOrder> q;
  bool isShutdown;
  thread* the;
};

struct TradingTool;
struct Exchange {
  Exchange() : engine(notif) {}

  void registerClient(uint16_t id, TradingTool* client);

  void start();

  void stop();

  Notifier notif;
  Engine engine;
};


