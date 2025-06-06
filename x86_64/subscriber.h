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

private:
  void decode();

  video_Frame *frame_;

  dds_entity_t participant_;
  dds_entity_t topic_;
  dds_entity_t reader_;
};