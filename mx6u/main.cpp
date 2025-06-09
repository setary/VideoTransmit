#include <cstring>

#include "publisher.h"
#include "subscriber.h"


int main(int argc, char** argv) {
  int type = 1; // 1 is publisher; 2 is subscriber
  int camera_id = 0;
  if (argc > 1) {
    if (strcmp("publisher", argv[1]) == 0) {
      type = 1;
    } else if (strcmp("subscriber", argv[1]) == 0) {
      type = 2;
    }
    if (argc == 3) {
      camera_id = atoi(argv[2]);
    }
  }

  switch (type)
  {
  case 1:
  {
    printf("this is a publisher.\n");
    VideoPublisher pub;
    pub.enable(camera_id);
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
