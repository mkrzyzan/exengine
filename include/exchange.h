#pragma once

#include <unordered_map>
#include <deque>
#include <map>
#include <thread>
#include <utility>
#include <limits>

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


struct Level 
{
  Level() : outstandingQty(0), openedOrdersQty(0) {}

  uint32_t outstandingQty, openedOrdersQty;
  deque<InternalOrder> orders;
};

struct Book 
{
  Book() : bid(numeric_limits<int>::min()), ask(numeric_limits<int>::max()) {}

  int bid, ask;

  map<int, Level, less<int>> bidLevels;
  map<int, Level, greater<int>> askLevels;
};

//=================== TRAITS ================
struct BuySideTrait
{
  static bool nonCross(const Book& book, int price) {return book.bid >= price;}
  static bool nonCrossOrInSpread(const Book& book, int price) {return book.ask > price;}

  static auto& GetCrossedLevels(Book& book) {return book.askLevels;}
  static auto& GetUncrossedLevels(Book& book) {return book.bidLevels;}

  static int& nonCrossBound(Book& book) {return book.bid;}

  static int& nonCrossOrSpreadBound(Book& book) {return book.ask;}
  static void resetNonCrossOrSpreadBound(Book& book) {book.ask = numeric_limits<int>::max();}

  static const Side side = Buy;
  static const Side otherSide = Sell;
};

struct SellSideTrait
{
  static bool nonCross(const Book& book, int price) {return book.ask <= price;}
  static bool nonCrossOrInSpread(const Book& book, int price) {return book.bid < price;}

  static auto& GetCrossedLevels(Book& book) {return book.bidLevels;}
  static auto& GetUncrossedLevels(Book& book) {return book.askLevels;}

  static int& nonCrossBound(Book& book) {return book.ask;}

  static int& nonCrossOrSpreadBound(Book& book) {return book.bid;}
  static void resetNonCrossOrSpreadBound(Book& book) {book.bid = numeric_limits<int>::min();}

  static const Side side = Sell;
  static const Side otherSide = Buy;
};
//=================== TRAITS ================




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

  template <typename SideTraits>
  void placeOrder(char instrument, int price, uint16_t trader, uint16_t qty);

  void placeOrder(char instrument, Side side, int price, uint16_t trader, uint16_t qty);



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


