#pragma once

#include <unordered_map>
#include <map>
#include <deque>
#include <thread>
#include <utility>

#include <threadable.h>
#include <connectors.h>

using namespace std;

enum Side {Buy, Sell, None};
enum EventType {OrderPlaced, Exec, Tick};

struct InternalOrder 
{
  InternalOrder(uint16_t trd, uint16_t qt, uint32_t pr) : trader(trd), origQty(qt), qty(qt), price(pr) {}
  uint16_t trader;
  uint16_t origQty;  // original order quantity
  uint16_t qty;      // remaining quantity
  uint32_t price;
};

struct InputOrder 
{
  char instrument;
  uint16_t trader;
  uint16_t qty;
  Side side;
  uint32_t price;
  bool operator==(const InputOrder& rhs)
  { 
    return instrument == rhs.instrument &&
           trader == rhs.trader &&
           qty == rhs.qty && 
           side == rhs.side &&
           price == rhs.price;
  }
};

struct Event 
{
  EventType type;
  char instrument;
  uint16_t trader;
  uint32_t qty;
  Side side;
  uint32_t price;      // For OrderPlaced/Exec: the order/execution price. For Tick: best bid
  uint32_t askPrice;   // For OrderPlaced/Exec: 0 (unused). For Tick: best ask

  bool operator==(const Event& rhs)
  { 
    return type == rhs.type &&
           instrument == rhs.instrument &&
           trader == rhs.trader &&
           qty == rhs.qty && 
           side == rhs.side &&
           price == rhs.price &&
           askPrice == rhs.askPrice;
  }
};

struct Book 
{
  Book() {}

  map<uint32_t, deque<InternalOrder>, greater<uint32_t>> buyOrders;  // descending price for buys
  map<uint32_t, deque<InternalOrder>> sellOrders;  // ascending price for sells
};

struct Notifier : public threadable
{
  Notifier();

  void registerClient(uint16_t id, SingleProducerSingleConsumerQueue<Event>* events);

  virtual void run();

  SingleProducerSingleConsumerQueue<Event> events;
  unordered_map<uint16_t, SingleProducerSingleConsumerQueue<Event>*> clients;
};

struct Engine : public threadable 
{
  Engine(Notifier& notifier);

  void placeOrder(char instrument, Side side, uint16_t trader, uint16_t qty, uint32_t price);

  void stop();

  virtual void run();

  Notifier& notify;
  unordered_map<char, Book> books;
  MultiProducerMultiConsumerQueue<InputOrder> q;
};

struct TradingTool;
struct Exchange 
{
  Exchange() : engine(notif) {}

  void registerClient(uint16_t id, TradingTool* client);

  void start();

  void stop();

  Notifier notif;
  Engine engine;
};


