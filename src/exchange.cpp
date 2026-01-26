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
      placeOrder(newOrder.instrument, newOrder.side, newOrder.trader, newOrder.qty, newOrder.price);
    }
  }
}

void Engine::placeOrder(char instrument, Side side, uint16_t trader, uint16_t qty, uint32_t price) 
{
  if (0 == qty || None == side) return;

  Book& book = books[instrument];
  uint16_t remainQty = qty;
  uint16_t executedQty = 0;

  // Try to match with opposite side orders
  if (side == Buy) {
    // Buy order: match with sell orders at or below our price
    auto it = book.sellOrders.begin();
    while (it != book.sellOrders.end() && remainQty > 0 && it->first <= price) {
      auto& orderQueue = it->second;
      while (!orderQueue.empty() && remainQty > 0) {
        InternalOrder& top = orderQueue.front();
        uint16_t matchQty = min(remainQty, top.qty);
        
        remainQty -= matchQty;
        executedQty += matchQty;
        top.qty -= matchQty;
        
        if (top.qty == 0) {
          if (false == notify.events.push({Exec, instrument, top.trader, top.origQty, Sell})) {
            cout << "ENGINE WARNING: events ring is full!. Increse the event buffer size!.\n";
            notify.events.forcePush({Exec, instrument, top.trader, top.origQty, Sell});
          }
          orderQueue.pop_front();
        }
      }
      
      if (orderQueue.empty()) {
        it = book.sellOrders.erase(it);
      } else {
        ++it;
      }
    }
    
    if (executedQty == qty) {
      // Fully executed
      if (false == notify.events.push({Exec, instrument, trader, qty, Buy})) {
        cout << "ENGINE WARNING: events ring is full!. Increse the event buffer size!.\n";
        notify.events.forcePush({Exec, instrument, trader, qty, Buy});
      }
    } else if (remainQty > 0) {
      // Partial or no execution - just place in book
      // When placing, use remainQty for the current qty, but keep track of original order qty
      book.buyOrders[price].emplace_back(trader, qty, price);
      book.buyOrders[price].back().qty = remainQty;  // Update qty to remaining amount
      if (false == notify.events.push({OrderPlaced, instrument, trader, qty, Buy})) {
        cout << "ENGINE WARNING: events ring is full!. Increse the event buffer size!.\n";
        notify.events.forcePush({OrderPlaced, instrument, trader, qty, Buy});
      }
    }
  } else {
    // Sell order: match with buy orders at or above our price
    auto it = book.buyOrders.begin();
    while (it != book.buyOrders.end() && remainQty > 0 && it->first >= price) {
      auto& orderQueue = it->second;
      while (!orderQueue.empty() && remainQty > 0) {
        InternalOrder& top = orderQueue.front();
        uint16_t matchQty = min(remainQty, top.qty);
        
        remainQty -= matchQty;
        executedQty += matchQty;
        top.qty -= matchQty;
        
        if (top.qty == 0) {
          if (false == notify.events.push({Exec, instrument, top.trader, top.origQty, Buy})) {
            cout << "ENGINE WARNING: events ring is full!. Increse the event buffer size!.\n";
            notify.events.forcePush({Exec, instrument, top.trader, top.origQty, Buy});
          }
          orderQueue.pop_front();
        }
      }
      
      if (orderQueue.empty()) {
        it = book.buyOrders.erase(it);
      } else {
        ++it;
      }
    }
    
    if (executedQty == qty) {
      // Fully executed
      if (false == notify.events.push({Exec, instrument, trader, qty, Sell})) {
        cout << "ENGINE WARNING: events ring is full!. Increse the event buffer size!.\n";
        notify.events.forcePush({Exec, instrument, trader, qty, Sell});
      }
    } else if (remainQty > 0) {
      // Partial or no execution - just place in book
      // When placing, use remainQty for the current qty, but keep track of original order qty
      book.sellOrders[price].emplace_back(trader, qty, price);
      book.sellOrders[price].back().qty = remainQty;  // Update qty to remaining amount
      if (false == notify.events.push({OrderPlaced, instrument, trader, qty, Sell})) {
        cout << "ENGINE WARNING: events ring is full!. Increse the event buffer size!.\n";
        notify.events.forcePush({OrderPlaced, instrument, trader, qty, Sell});
      }
    }
  }

  // market data - calculate total outstanding qty across all price levels
  uint32_t totalOutstanding = 0;
  Side bestSide = None;
  
  if (!book.buyOrders.empty()) {
    for (const auto& priceLevel : book.buyOrders) {
      for (const auto& order : priceLevel.second) {
        totalOutstanding += order.qty;
      }
    }
    bestSide = Buy;
  }
  
  if (!book.sellOrders.empty()) {
    uint32_t sellQty = 0;
    for (const auto& priceLevel : book.sellOrders) {
      for (const auto& order : priceLevel.second) {
        sellQty += order.qty;
      }
    }
    if (sellQty > 0) {
      totalOutstanding += sellQty;
      bestSide = (bestSide == Buy) ? None : Sell;
    }
  }
  
  if (false == notify.events.push({Tick, instrument, 0, totalOutstanding, bestSide})) {
    cout << "ENGINE WARNING: events ring is full!. Increse the event buffer size!.\n";
    notify.events.forcePush({Tick, instrument, 0, totalOutstanding, bestSide});
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


