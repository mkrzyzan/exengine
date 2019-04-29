#include <tradingtool.h>


TradingTool::TradingTool(uint16_t identifier) : id(identifier) {}

void TradingTool::connectTo(Exchange& ex)
{
  q = &ex.engine.q; 
  ex.registerClient(id, this);
}

void TradingTool::run() 
{
  if (init) init(this);
  while (false == isShutdown)
  {
    Event event;
    if (true == events.pop(event))
    {
      if (algo) algo(this,event);
    }
    else
    {
      this_thread::yield(); // not needed if spin lock
    }
  }
}

