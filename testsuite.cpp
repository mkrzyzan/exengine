#include <iostream>
#include <thread>
#include <utility>
#include <mutex>
#include <condition_variable>

#include <connectors.h>
#include <exchange.h>
#include <tradingtool.h>

//tests
#include "gtest/gtest.h"
using namespace std;

TEST(MatchingEngineTest, FourOrders)
{
  Exchange ex;
  Notifier& notif = ex.notif;
  Engine& eng = ex.engine;
  Event event;

  eng.placeOrder('A', Buy, 666, 100, 100);
  eng.placeOrder('A', Buy, 777, 200, 100);
  eng.placeOrder('A', Sell, 888, 200, 100);
  eng.placeOrder('A', Sell, 888, 100, 100);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'A',666,100,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,100,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'A',777,200,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,300,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',666,100,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',888,200,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,100,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',777,200,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',888,100,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,0,None,0}) == event);
  ASSERT_TRUE (false == notif.events.pop(event));
}

TEST(MatchingEngineTest, CasesFromEmail)
{
  Exchange ex;
  Notifier& notif = ex.notif;
  Engine& eng = ex.engine;
  Event event;

  eng.placeOrder('S', Buy, 1, 200, 100);
  eng.placeOrder('S', Sell, 2, 200, 100);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',1,200,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,200,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',1,200,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',2,200,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,0,None,0}) == event);

  eng.placeOrder('G', Sell, 3, 300, 100);
  eng.placeOrder('G', Buy, 4, 200, 100);
  eng.placeOrder('G', Buy, 5, 200, 100);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'G',3,300,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'G',0,300,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'G',4,200,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'G',0,100,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'G',3,300,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'G',5,200,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'G',0,100,Buy,100}) == event);

  eng.placeOrder('H', Sell, 6, 200, 100);
  eng.placeOrder('H', Sell, 7, 200, 100);
  eng.placeOrder('H', Sell, 8, 200, 100);
  eng.placeOrder('H', Buy, 9, 600, 100);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'H',6,200,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,200,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'H',7,200,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,400,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'H',8,200,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,600,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',6,200,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',7,200,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',8,200,Sell,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',9,600,Buy,100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,0,None,0}) == event);
  ASSERT_TRUE (false == notif.events.pop(event));
}

TEST(MatchingEngineTest, OverEatingOneSide)
{
  Exchange ex;
  Notifier& notif = ex.notif;
  Engine& eng = ex.engine;
  Event event;

  for (Side side : {Buy, Sell}) 
  {
    Side otherSide = (Buy == side) ? Sell : Buy;

    eng.placeOrder('S', side, 1, 100, 100);
    eng.placeOrder('S', side, 2, 200, 100);
    eng.placeOrder('S', side, 3, 300, 100);
    eng.placeOrder('S', side, 4, 400, 100);
    eng.placeOrder('S', side, 5, 500, 100);
    eng.placeOrder('S', otherSide, 6, 100, 100);
    eng.placeOrder('S', otherSide, 7, 100, 100);
    eng.placeOrder('S', otherSide, 8, 100, 100);
    eng.placeOrder('S', otherSide, 9, 150, 100);
    eng.placeOrder('S', otherSide, 10, 300, 100);
    eng.placeOrder('S', otherSide, 11, 100, 100);
    eng.placeOrder('S', otherSide, 12, 700, 100);
    eng.placeOrder('S', side, 13, 50, 100);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',1,100,side,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,100,side,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',2,200,side,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,300,side,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',3,300,side,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,600,side,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',4,400,side,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1000,side,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',5,500,side,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1500,side,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',1,100,side,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',6,100,otherSide,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1400,side,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',7,100,otherSide,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1300,side,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',2,200,side,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',8,100,otherSide,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1200,side,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',9,150,otherSide,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1050,side,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',3,300,side,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',10,300,otherSide,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,750,side,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',11,100,otherSide,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,650,side,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',4,400,side,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',5,500,side,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',12,700,otherSide,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,50,otherSide,100}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',12,700,otherSide,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',13,50,side,100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,0,None,0}) == event);

    ASSERT_TRUE (false == notif.events.pop(event));
  }
  
  //assert (true == notif.events.pop(event)); 
  //cout << "type="<<event.type << ", instrument="<<event.instrument << ", trader=" << event.trader <<", qt=" << event.qty<<  endl;;
}


TEST(MatchingEngineTest, OrderSlicing)
{


  Exchange ex;
  Notifier& notif = ex.notif;
  Engine& eng = ex.engine;
  Event event;

  eng.placeOrder('H', Buy, 3, 5, 100);
  eng.placeOrder('H', Sell, 1, 11, 100);
  eng.placeOrder('H', Buy, 2, 7, 100);
  eng.placeOrder('H', Buy, 3, 5, 100);
  eng.placeOrder('H', Sell, 1, 11, 100);
  eng.placeOrder('H', Buy, 3, 5, 100);


  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced, 'H', 3, 5, Buy, 100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 5, Buy, 100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 3, 5, Buy, 100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced, 'H', 1, 11, Sell, 100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 6, Sell, 100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 1, 11, Sell, 100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced, 'H', 2, 7, Buy, 100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 1, Buy, 100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced, 'H', 3, 5, Buy, 100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 6, Buy, 100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 2, 7, Buy, 100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 3, 5, Buy, 100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced, 'H', 1, 11, Sell, 100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 5, Sell, 100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 1, 11, Sell, 100}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 3, 5, Buy, 100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 0, None, 0}) == event);
  ASSERT_FALSE ( notif.events.pop(event));
}


class MatchingEnginePerformance : public testing::TestWithParam<uint16_t> {};

TEST_P(MatchingEnginePerformance, EventsBurst)
{
  Exchange ex;
  Notifier& notif = ex.notif;
  Engine& eng = ex.engine;
  Event event;

  //  (100 * 3 * GetParam()) + 2 events
  for (int n = 0; n < 100; n++)
  {
    uint32_t qty = 0;
    for (int i = 0; i < GetParam(); i++)
    {
      eng.placeOrder('H', Buy, 1, 1, 100);
      ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced, 'H', 1, 1, Buy, 100}) == event);
      qty += 1;
      ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, qty, Buy, 100}) == event);
    }

    eng.placeOrder('H', Sell, 2, GetParam(), 100);
    for (int i = 0; i < GetParam(); i++)
    {
      ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 1, 1, Buy, 100}) == event);
    }
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 2, GetParam(), Sell, 100}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick, 'H', 0, 0, None, 0}) == event);
    ASSERT_TRUE (false == notif.events.pop(event));
  }
}

INSTANTIATE_TEST_SUITE_P(Perfo, MatchingEnginePerformance, testing::Values(1<<1, 1<<3, 1<<5, 1<<7, 1<<9, 1<<11, 1<<13, 1<<15, (1<<16)-2),
                        testing::PrintToStringParamName());


TEST(MultiProducerMultiConsumerQueueTest, OneThread_perf)
{
  MultiProducerMultiConsumerQueue<InputOrder> q; 
  InputOrder order;

  for (uint32_t i = 0; i < 1000000; i++)
  {
    uint16_t x = static_cast<uint16_t>(i);
    q.push(InputOrder{'A', x,x, (x % 2) ? Buy : Sell, 100});

    if (i >= 10000)
    {
      q.pop(order);
      ASSERT_TRUE ((InputOrder{'A', static_cast<uint16_t>(x-10000), static_cast<uint16_t>(x-10000), (x % 2) ? Buy : Sell, 100}) == order);
    }
  }

  for (int i = 0; i < 10000; i++) q.pop(order);
}

TEST(MultiProducerMultiConsumerQueueTest, TwoThreads_perf)
{
  MultiProducerMultiConsumerQueue<InputOrder> q; 
  thread t1([&]() {
    for (uint32_t i = 0; i < 1000000; i++)
    {
      uint16_t x = static_cast<uint16_t>(i);
      q.push(InputOrder{'A', x, x, (x % 2) ? Buy : Sell, 100});
    }
  });

  for (uint32_t i = 0; i < 1000000; i++)
  {
    uint16_t x = static_cast<uint16_t>(i);
    InputOrder order;
    q.pop(order);
    ASSERT_TRUE ((InputOrder{'A', x, x, (x % 2) ? Buy : Sell, 100}) == order);
  }

  t1.join();
}


TEST(SingleProducerSingleConsumerQueueTest, TwoThreads_perf)
{
  SingleProducerSingleConsumerQueue<Event,800> q;
  thread t1([&]() {
    uint32_t c = 0;
    while (c < 1000000)
    {
      uint16_t x = static_cast<uint16_t>(c);
      if (false == q.push(Event{Exec, 'A', x, x, (x % 2) ? Buy : Sell, 100}))
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
      //ASSERT_TRUE ((Event{Exec, 'A', x, x, (x % 2) ? Buy : Sell, 100}) == event);
      ASSERT_EQ (Exec, event.type);
      ASSERT_EQ ('A', event.instrument);
      ASSERT_EQ (x, event.trader);
      ASSERT_EQ (x, event.qty);
      ASSERT_EQ ( (x%2) ? Buy : Sell, event.side);
      ASSERT_EQ (100u, event.price);

      c++;
    }
  }

  t1.join();
}

class IntegrationTest : public ::testing::Test
{
public:
  IntegrationTest() : trader1(1), trader2(2), trader3(3) {}
  virtual ~IntegrationTest() {}
  virtual void SetUp() {}
  virtual void TearDown() {}

  void start() {
    trader1.connectTo(ex);
    trader2.connectTo(ex);
    trader3.connectTo(ex);
    ex.start();
    trader1.start();
    trader2.start();
    trader3.start();
  }

  void stop() {
    trader1.stop();
    trader2.stop();
    trader3.stop();
    ex.stop();
  }

  Exchange ex;
  TradingTool trader1, trader2, trader3;
  mutex m;
  condition_variable cv;
};


TEST_F(IntegrationTest, OneTraderConnectedToExchange)
{
  bool orderAccepted = false;

  auto init = [](TradingTool* me){
    me->q->push(InputOrder{'H', me->id, 10, Sell, 100});
  };

  auto algo = [&](TradingTool* me, Event e){
    switch(e.type)
    {
      case EventType::Exec:
      {
        break;
      }
      case EventType::OrderPlaced:
      {
        unique_lock<mutex> lc(m);
        orderAccepted = true;
        cv.notify_all();
        break;
      }
    }
  };

  trader1.init = init; trader1.algo = algo;

  start();

  {
    unique_lock<mutex> lc(m);
    cv.wait_for(lc, 300ms, [&](){ return true == orderAccepted; });
  }

  stop();

  ASSERT_TRUE (orderAccepted);
}

TEST_F(IntegrationTest, TwoTraderConnectedToExchange)
{
  bool orderAccepted = false;
  bool orderExec1 = false;
  bool orderExec2 = false;

  auto init = [](TradingTool* me){
    me->q->push(InputOrder{'H', me->id, 10, (me->id % 2) ? Sell : Buy, 100});
  };

  auto algo = [&](TradingTool* me, Event e){
    switch(e.type)
    {
      case EventType::Exec:
      {
        unique_lock<mutex> lc(m);
        (me->id % 2) ? (orderExec1 = true) : (orderExec2 = true);
        cv.notify_all();
        break;
      }
      case EventType::OrderPlaced:
      {
        unique_lock<mutex> lc(m);
        orderAccepted = true;
        cv.notify_all();
        break;
      }
    }
  };

  trader1.init = init; trader1.algo = algo;
  trader2.init = init; trader2.algo = algo;
  
  start();

  {
    unique_lock<mutex> lc(m);
    cv.wait_for(lc, 300ms, [&](){ return (true == orderAccepted) && (true == orderExec1) && (true == orderExec2); });
  }

  stop();

  ASSERT_TRUE (orderAccepted);
  ASSERT_TRUE (orderExec1);
  ASSERT_TRUE (orderExec2);
}

TEST_F(IntegrationTest, ThreeTraderConnectedToExchange_OrderSlicing_15250_orders)
{

  struct alignas(64) D {uint16_t slice; uint32_t toExec; Side s; bool done;};
  map<uint16_t,D> traderNotebook = {{1, {25, 100000, Sell, false}}, 
                                    {2, {56, 70000, Buy, false}}, 
                                    {3, {3, 30000, Buy, false}}};

  auto init = [&](TradingTool* me){
    //unique_lock<mutex> lc(m);
    //cout << "AddOrder: trader= " << me->id << endl;
    me->q->push(InputOrder{'H', me->id, traderNotebook[me->id].slice, traderNotebook[me->id].s, 100});

    /*
    cout << "eng.placeOrder('" << 'H' 
         << ", " << ((Buy == traderNotebook[me->id].s) ? "Buy" : "Sell")
         << ", " << me->id
         << ", " << traderNotebook[me->id].slice
         << ");" << endl;
    */
  };

  auto algo = [&](TradingTool* me, Event e){
    switch(e.type)
    {
      case EventType::Exec:
      {
        uint32_t& toExec = traderNotebook[me->id].toExec;
        toExec -= e.qty;
        {
          /*
          cout << "(Event{Exec, '" << e.instrument
               << "', " << e.trader
               << ", " << e.qty
               << ", " << ((Buy == e.side) ? "Buy" : "Sell")
               << "}) == event)" << endl;
          */
        }
        if (toExec != 0)
        {
          me->q->push(InputOrder{'H', me->id, traderNotebook[me->id].slice, traderNotebook[me->id].s, 100});

          /*
          cout << "eng.placeOrder('" << e.instrument
               << ", " << ((Buy == traderNotebook[me->id].s) ? "Buy" : "Sell")
               << ", " << me->id
               << ", " << traderNotebook[me->id].qt
               << ");" << endl;
          */
        }
        else
        {
          //cout << "Trader = " << me->id << ", finish\n";
          unique_lock<mutex> lc(m);
          traderNotebook[me->id].done = true;
          cv.notify_all();
        }
        break;
      }
      case EventType::OrderPlaced:
      {
        /*
        cout << "(Event{OrderPlaced, '" << e.instrument
             << "', " << e.trader
             << ", " << e.qty
             << ", " << ((Buy == e.side) ? "Buy" : "Sell")
             << "}) == event)" << endl;
        */

        break;
      }
    }
  };

  trader1.init = init; trader1.algo = algo;
  trader2.init = init; trader2.algo = algo;
  trader3.init = init; trader3.algo = algo;
  
  start();

  {
    unique_lock<mutex> lc(m);
    cv.wait_for(lc, 1000ms, [&](){ return traderNotebook[1].done && traderNotebook[2].done && traderNotebook[3].done; });
  }

  stop();

  ASSERT_TRUE (traderNotebook[1].done);
  ASSERT_TRUE (traderNotebook[2].done);
  ASSERT_TRUE (traderNotebook[3].done);
}

TEST(MatchingEngineTest, OrderPriceParameter)
{
  Exchange ex;
  Notifier& notif = ex.notif;
  Engine& eng = ex.engine;
  Event event;

  // Test price-time priority matching
  eng.placeOrder('L', Buy, 1, 100, 50);
  eng.placeOrder('L', Buy, 2, 200, 75);
  eng.placeOrder('L', Sell, 3, 150, 50);

  // Verify events generated
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'L',1,100,Buy,50}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'L',0,100,Buy,50}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'L',2,200,Buy,75}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'L',0,300,Buy,75}) == event);
  // Sell @50 matches with best buy (trader 2 @75) for 150 qty
  // Trader 2's order is partially filled (150 of 200), so only the sell generates Exec
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'L',3,150,Sell,50}) == event);
  // Remaining: trader 2 has 50 @75, trader 1 has 100 @50 = 150 total
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'L',0,150,Buy,75}) == event);
  ASSERT_FALSE (notif.events.pop(event));
}


//========================   MAIN MAIN  ==============================
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
