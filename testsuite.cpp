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

  eng.placeOrder('A', Buy, 666, 100);
  eng.placeOrder('A', Buy, 777, 200);
  eng.placeOrder('A', Sell, 888, 200);
  eng.placeOrder('A', Sell, 888, 100);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'A',666,100,Buy}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,100,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'A',777,200,Buy}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,300,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',666,100,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',888,200,Sell}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,100,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',777,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'A',888,100,Sell}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'A',0,0,None}) == event);
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
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',1,200,Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',2,200,Sell}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,0,None}) == event);

  eng.placeOrder('G', Sell, 3, 300);
  eng.placeOrder('G', Buy, 4, 200);
  eng.placeOrder('G', Buy, 5, 200);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'G',3,300,Sell}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'G',0,300,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'G',4,200,Buy}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'G',0,100,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'G',3,300,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'G',5,200,Buy}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'G',0,100,Buy}) == event);

  eng.placeOrder('H', Sell, 6, 200);
  eng.placeOrder('H', Sell, 7, 200);
  eng.placeOrder('H', Sell, 8, 200);
  eng.placeOrder('H', Buy, 9, 600);

  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'H',6,200,Sell}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'H',7,200,Sell}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,400,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'H',8,200,Sell}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,600,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',6,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',7,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',8,200,Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'H',9,600,Buy}) == event);
  //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H',0,0,None}) == event);
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

    eng.placeOrder('S', side, 1, 100);
    eng.placeOrder('S', side, 2, 200);
    eng.placeOrder('S', side, 3, 300);
    eng.placeOrder('S', side, 4, 400);
    eng.placeOrder('S', side, 5, 500);
    eng.placeOrder('S', otherSide, 6, 100);
    eng.placeOrder('S', otherSide, 7, 100);
    eng.placeOrder('S', otherSide, 8, 100);
    eng.placeOrder('S', otherSide, 9, 150);
    eng.placeOrder('S', otherSide, 10, 300);
    eng.placeOrder('S', otherSide, 11, 100);
    eng.placeOrder('S', otherSide, 12, 700);
    eng.placeOrder('S', side, 13, 50);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',1,100,side}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,100,side}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',2,200,side}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,300,side}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',3,300,side}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,600,side}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',4,400,side}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1000,side}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',5,500,side}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1500,side}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',1,100,side}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',6,100,otherSide}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1400,side}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',7,100,otherSide}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1300,side}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',2,200,side}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',8,100,otherSide}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1200,side}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',9,150,otherSide}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,1050,side}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',3,300,side}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',10,300,otherSide}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,750,side}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',11,100,otherSide}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,650,side}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',4,400,side}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',5,500,side}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced,'S',12,700,otherSide}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,50,otherSide}) == event);

    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',12,700,otherSide}) == event);
    ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec,'S',13,50,side}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'S',0,0,None}) == event);

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

  eng.placeOrder('H', Buy, 3, 5);
  eng.placeOrder('H', Sell, 1, 11);
  eng.placeOrder('H', Buy, 2, 7);
  eng.placeOrder('H', Buy, 3, 5);
  eng.placeOrder('H', Sell, 1, 11);
  eng.placeOrder('H', Buy, 3, 5);


  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced, 'H', 3, 5, Buy}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 5, Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 3, 5, Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced, 'H', 1, 11, Sell}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 6, Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 1, 11, Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced, 'H', 2, 7, Buy}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 1, Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced, 'H', 3, 5, Buy}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 6, Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 2, 7, Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 3, 5, Buy}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{OrderPlaced, 'H', 1, 11, Sell}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 5, Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 1, 11, Sell}) == event);
  ASSERT_TRUE (true == notif.events.pop(event) && (Event{Exec, 'H', 3, 5, Buy}) == event);
    //ASSERT_TRUE (true == notif.events.pop(event) && (Event{Tick,'H', 0, 0, None}) == event);
  ASSERT_FALSE ( notif.events.pop(event));
}


TEST(MultiProducerMultiConsumerQueueTest, OneThread_perf)
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

TEST(MultiProducerMultiConsumerQueueTest, TwoThreads_perf)
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


TEST(SingleProducerSingleConsumerQueueTest, TwoThreads_perf)
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
      //ASSERT_TRUE ((Event{Exec, 'A', x, x, (x % 2) ? Buy : Sell}) == event);
      ASSERT_EQ (Exec, event.type);
      ASSERT_EQ ('A', event.instrument);
      ASSERT_EQ (x, event.trader);
      ASSERT_EQ (x, event.qty);
      ASSERT_EQ ( (x%2) ? Buy : Sell, event.side);

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
    me->q->push(InputOrder{'H', me->id, 10, Sell});
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
    me->q->push(InputOrder{'H', me->id, 10, (me->id % 2) ? Sell : Buy});
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
    me->q->push(InputOrder{'H', me->id, traderNotebook[me->id].slice, traderNotebook[me->id].s});

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
          me->q->push(InputOrder{'H', me->id, traderNotebook[me->id].slice, traderNotebook[me->id].s});

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
    cv.wait_for(lc, 5s, [&](){ return traderNotebook[1].done && traderNotebook[2].done && traderNotebook[3].done; });
  }

  stop();

  ASSERT_TRUE (traderNotebook[1].done);
  ASSERT_TRUE (traderNotebook[2].done);
  ASSERT_TRUE (traderNotebook[3].done);
}


//========================   MAIN MAIN  ==============================
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
