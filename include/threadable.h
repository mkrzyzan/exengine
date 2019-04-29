#pragma once
#include <thread>
using namespace std;

struct threadable
{ 
  threadable(); 

  ~threadable(); 

  void start();

  void stop();

  virtual void run() = 0;

  thread* the;
  bool isShutdown;
};

