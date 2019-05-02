#include <exchange.h>
#include <tradingtool.h>
#include <iostream>
using namespace std;

Notifier::Notifier() {}

void Notifier::run() 
{
  while (false == isShutdown) {
    Event event;
    if (true == events.pop(event))
    {
      switch(event.type)
      {
        case EventType::Exec:
        case EventType::OrderPlaced:
        {
          if (false == clients[event.trader]->push(event))
          {
            cout << "NOTIFIER WARNING: events ring is full!. Increse the clients event buffer size!.\n";
            clients[event.trader]->forcePush(event);
          }
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

void Notifier::registerClient(uint16_t id, SingleProducerSingleConsumerQueue<Event>* events) 
{
  clients[id] = events;
}

void Engine::stop() 
{
  q.stop();
  threadable::stop();
}

Engine::Engine(Notifier& notifier) : books(), notify(notifier) {}

void Engine::run() 
{
  while (false == isShutdown)
  {
    // blocking call
    InputOrder newOrder;
    if (true == q.pop(newOrder)) 
    {
      placeOrder(newOrder.instrument, newOrder.side, newOrder.trader, newOrder.qty);
    }
  }
}


template <typename SideTraits>
void Engine::placeOrder(char instrument, int price, uint16_t trader, uint16_t qty) 
{
  Book& book = books[instrument];
  uint16_t remainQty = qty;
  
  if (true == SideTrait::nonCrossOrInSpread(book, price)
  {
    if (false == SideTrait::nonCross(book, price)
    {
      int& nonCrossBound = SideTrait::nonCrossBound();
      nonCrossBound = price;
    }

    //find limit
    // Y - add order to limit
    // N - create limit, add order to limit
    auto& levels = SideTrait::GetUncrossedLevels(book);

    auto it = evels.lower_bound(price);

    if (it->first != price)
    {
      //create limit
      it = levels.emplace_hint(it, price);
    }

    it->second.orders.emplace_back(trader, qty);
    it->second.orders.outstandingQty += qty;
    it->second.orders.openedOrdersQty += qty;

  }
  else
  {
    // crossed
    auto& levels = SideTrait::GetCrossedLevels(book);

    // walk through levels
    auto it = levels.begin();
    while (it != levels.end() && 0 != remainQty)
    {
      auto& level = it->second;
      auto& levelPrice = it->first;

      // walk through orders @ level
      while (false == level.orders.empty() && 0 != remainQty) 
      {
        InternalOrder& top = level.orders.front();
        uint32_t topRemainQty = (top.qty + level.outstandingQty) - level.openedOrdersQty;
        if (topRemainQty > remainQty)
        {
          level.outstandingQty -= remainQty;
          remainQty = 0;
        }
        else 
        {
          remainQty -= topRemainQty;
          level.orders.pop_front();
          level.outstandingQty -= topRemainQty;
          level.openedOrdersQty -= top.qty;

          notify.events.forcePush({Exec, instrument, top.trader, top.qty, level.actualSide});
        }
      }
      
      // if all orders from level have got eaten, erase the level
      if (0 != remainQty)
      {
        it = levels.erase(it);
      }
    }


    if (0 == remainQty)
    {
      notify.events.forcePush({Exec, instrument, trader, qty, side});

      // update maxBid, minAsk
      SideTrait::nonCrossOrSpreadBound() = levelPrice;

    }
    else
    {
      level.actualSide = side;
      level.orders.emplace_back(trader, qty);
      level.outstandingQty += remainQty;
      level.openedOrdersQty += qty;
      notify.events.forcePush({OrderPlaced, instrument, trader, qty, side});

      // update maxBid, minAsk
      SideTrait::nonCrossOrSpreadBound() = 0;
      SideTrait::nonCrossBound() = levelPrice;
    }
  }
}

void Engine::placeOrder(char instrument, Side side, int price,  uint16_t trader, uint16_t qty) 
{
  if (Sell == side)
    placeOrder<SellSideTrait>(instrument, price, trader, qty);
  else
    placeOrder<BuySideTrait>(instrument, price, trader, qty);
}


  // market data
  if (false == book.orders.empty())
  {
    if (false == notify.events.push({Tick, instrument, 0, book.outstandingQty, book.actualSide}))
    {
      cout << "ENGINE WARNING: events ring is full!. Increse the event buffer size!.\n";
      notify.events.forcePush({Tick, instrument, 0, book.outstandingQty, book.actualSide});
    }
  }
  else
  {
    if (false == notify.events.push({Tick, instrument, 0, 0, None}))
    {
      cout << "ENGINE WARNING: events ring is full!. Increse the event buffer size!.\n";
      notify.events.forcePush({Tick, instrument, 0, 0, None});
    }
  }
}


void Exchange::registerClient(uint16_t id, TradingTool* client) 
{
  notif.registerClient(id, &client->events);
}

void Exchange::start() 
{
  engine.start();
  notif.start();
}

void Exchange::stop() 
{
  notif.stop();
  engine.stop();
}


