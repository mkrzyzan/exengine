#pragma once
#include <connectors.h>
#include <exchange.h>
#include <functional>
using namespace std;

struct TradingTool {
  using gateway = MultiProducerMultiConsumerQueue<InputOrder>; 

  TradingTool(uint16_t identifier);

  SingleProducerSingleConsumerQueue<Event> events;

  void connectTo(Exchange& ex);

  void start();

  void stop();

  void run(); 

  gateway* q;
  uint16_t id;
  bool isShutdown;
  thread* the;
  function<void(TradingTool*,Event)> algo;
  function<void(TradingTool*)> init;
};

