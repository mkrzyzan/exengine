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
  Book() {}

  map<int, Level, greater<int>> bidLevels;
  map<int, Level, less<int>> askLevels;
};

//=================== TRAITS ================
struct SideTrait
{
  static int GetAsk(const Book& book) {
    return (true == book.askLevels.empty()) ? numeric_limits<int>::max() : book.askLevels.begin()->first;
  }

  static int GetBid(const Book& book) {
    return (true == book.bidLevels.empty()) ? numeric_limits<int>::min() : book.bidLevels.begin()->first;
  }
};

struct BuySideTrait : SideTrait
{
  static bool liquidityProvider(const Book& book, int price) {return GetAsk(book) > price;}

  static auto& GetCrossedLevels(Book& book) {return book.askLevels;}
  static auto& GetUncrossedLevels(Book& book) {return book.bidLevels;}

  static const Side side = Buy;
  static const Side otherSide = Sell;
};

struct SellSideTrait : SideTrait
{
  static bool liquidityProvider(const Book& book, int price) {return GetBid(book) < price;}

  static auto& GetCrossedLevels(Book& book) {return book.bidLevels;}
  static auto& GetUncrossedLevels(Book& book) {return book.askLevels;}

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


