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

void Engine::placeOrder(char instrument, Side side, uint16_t trader, uint16_t qty) 
{
  Book& book = books[instrument];
  uint16_t remainQty = qty;


  if (true == book.orders.empty() || side == book.actualSide) {
    book.actualSide = side;
    book.orders.emplace_back(trader, qty);
    book.outstandingQty += qty;
    book.openedOrdersQty += qty;

    if (false == notify.events.push({OrderPlaced, instrument, trader, qty, side}))
    {
      cout << "ERROR: events ring is full, event drop!. Increse the event buffer size!.";
    }
  }
  else
  {
    while (false == book.orders.empty() && 0 != remainQty) {
      InternalOrder& top = book.orders.front();
      uint32_t topRemainQty = (top.qty + book.outstandingQty) - book.openedOrdersQty;
      if (topRemainQty > remainQty)
      {
        book.outstandingQty -= remainQty;
        remainQty = 0;
      }
      else {
        remainQty -= topRemainQty;
        book.orders.pop_front();
        book.outstandingQty -= topRemainQty;
        book.openedOrdersQty -= top.qty;

        if (false == notify.events.push({Exec, instrument, top.trader, top.qty, book.actualSide}))
        {
          cout << "ERROR: events ring is full, event drop!. Increse the event buffer size!.";
        }
      }
    }

    if (0 == remainQty)
    {
      if (false == notify.events.push({Exec, instrument, trader, qty, side}))
      {
        cout << "ERROR: events ring is full, event drop!. Increse the event buffer size!.";
      }
    }
    else
    {
      book.actualSide = side;
      book.orders.emplace_back(trader, qty);
      book.outstandingQty += remainQty;
      book.openedOrdersQty += qty;
      if (false == notify.events.push({OrderPlaced, instrument, trader, qty, side}))
      {
        cout << "ERROR: events ring is full, event drop!. Increse the event buffer size!.";
      }
    }
  }

  // market data
  if (false == book.orders.empty())
  {
    if (false == notify.events.push({Tick, instrument, 0, book.outstandingQty, book.actualSide}))
    {
      cout << "ERROR: events ring is full, event drop!. Increse the event buffer size!.";
    }
  }
  else
  {
    if (false == notify.events.push({Tick, instrument, 0, 0, None}))
    {
      cout << "ERROR: events ring is full, event drop!. Increse the event buffer size!.";
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


