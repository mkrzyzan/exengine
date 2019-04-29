#include <threadable.h>
#include <thread>
using namespace std;

threadable::threadable() : the(nullptr), isShutdown(false) {}

threadable::~threadable()
{
  stop();
}

void threadable::start() 
{
  the = new thread([&](){this->run();});
}

void threadable::stop() 
{
  isShutdown = true;
  if (the) the->join();
  delete the;
  the = nullptr;
}
 
