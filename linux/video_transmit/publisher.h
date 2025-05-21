#include <stdio.h>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>

#include "videoPubSubTypes.h"


class VideoPublisher {
public:
  VideoPublisher();
  ~VideoPublisher();

  bool enable();

  bool disable();

  void run();

private:
  bool capture();

  void compress();

  void encode();

  bool publish();

  void runThread();

  cv::VideoCapture cap_;
  cv::Mat image_;

  video::Frame frame_;
  eprosima::fastdds::dds::DomainParticipant* participant_;
  eprosima::fastdds::dds::Publisher* publisher_;
  eprosima::fastdds::dds::Topic* topic_;
  eprosima::fastdds::dds::DataWriter* writer_;
  eprosima::fastdds::dds::TypeSupport type_;
};