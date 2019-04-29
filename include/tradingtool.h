#pragma once
#include <connectors.h>
#include <exchange.h>
#include <functional>
using namespace std;

struct TradingTool : public threadable 
{
  using gateway = MultiProducerMultiConsumerQueue<InputOrder>; 

  TradingTool(uint16_t identifier);

  SingleProducerSingleConsumerQueue<Event> events;

  void connectTo(Exchange& ex);

  void virtual run(); 

  gateway* q;
  uint16_t id;
  function<void(TradingTool*,Event)> algo;
  function<void(TradingTool*)> init;
};

