#include <stdio.h>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include "dds/dds.h"

#include "video.h"


class VideoSubscriber {
public:
  VideoSubscriber();
  ~VideoSubscriber();

  bool enable();

  bool disable();

  void run();

  void decode();
  void* samples[1];
  video_Frame* frame_;

private:
  dds_entity_t participant_;
  dds_entity_t topic_;
  dds_entity_t reader_;
};