#pragma once

#include <unordered_map>
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
  InternalOrder(uint16_t trd, uint16_t qt) : trader(trd), qty(qt) {}
  uint16_t trader;
  uint16_t qty;
};

struct InputOrder 
{
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

struct Event 
{
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

//=================== TRAITS ================
struct BuySideTrait
{
  static bool nonCross(const Book& book, int price) {return ask <= price;}
  static bool nonCrossOrInSpread(const Book& book, int price) {return bid <= price;}
};

struct SellSideTrait
{
  static bool nonCross(const Book& book, int price) {return bid >= price;}
  static bool nonCrossOrInSpread(const Book& book, int price) {return ask >= price;}
};
/
//=================== TRAITS ================


struct Level 
{
  Limit() : outstandingQty(0), openedOrdersQty(0) {}

  uint32_t outstandingQty, openedOrdersQty;
  deque<InternalOrder> orders;
};

struct Book 
{
  Book() : bidMax(0), askMax(0) {}

  int bidMax, askMax;

  map<int, Level, less<int>> bidLevels;
  map<int, Level, greater<int>> askLevels;
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

  void placeOrder(char instrument, Side side, uint16_t trader, uint16_t qty);

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


