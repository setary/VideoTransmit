#include <cstring>

#include "subscriber.h"


int main(int argc, char** argv) {
  printf("this is a subscriber.\n");
  VideoSubscriber sub;
  sub.enable();
  sub.run();

  return 0;
}