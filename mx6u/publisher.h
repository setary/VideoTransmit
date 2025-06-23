#include <stdio.h>
#include <unistd.h>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "dds/dds.h"

#include "video.h"


class VideoPublisher {
public:
  VideoPublisher();
  ~VideoPublisher();

  bool enable(int camera_id);

  bool disable();

  void run();

private:
  bool capture();

  bool encode();

  bool publish(uint8_t* data, int dataLen);

  static void* runThread(void* arg);

  cv::VideoCapture cap_;
  cv::Mat image_;
  std::vector<uint8_t> jpeg_data_;

  video_Frame frame_;

  dds_entity_t participant_;
  dds_entity_t topic_;
  dds_entity_t writer_;
};
