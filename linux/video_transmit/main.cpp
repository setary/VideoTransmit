#include <cstring>

#include "publisher.h"
#include "subscriber.h"


int main(int argc, char** argv) {
  int type = 1; // 1 is publisher; 2 is subscriber
  if (argc) {
    if (strcmp("publisher", argv[1]) == 0) {
      type = 1;
    } else if (strcmp("subscriber", argv[1]) == 0) {
      type = 2;
    }
  }

  switch (type)
  {
  case 1:
  {
    printf("this is a publisher.\n");
    VideoPublisher pub;
    pub.enable();
    pub.run();
  }
    break;

  case 2:
  {
    printf("this is a subscriber.\n");
    VideoSubscriber sub;
    sub.enable();
    sub.run();
  }
    break;
  
  default:
    break;
  }

  return 0;
}