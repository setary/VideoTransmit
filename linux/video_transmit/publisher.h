#include <stdio.h>
#include <unistd.h>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
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

  bool encode();

  int rtpPack(int offset);

  bool publish(uint8_t* data, int dataLen);

  void runThread();

  cv::VideoCapture cap_;
  cv::Mat image_;
  std::vector<uint8_t> jpeg_data_;
  std::vector<uint8_t> rtp_pkt_;
  uint16_t seq_no_ = 0;
  uint32_t timestamp_ = 0;

  video::Frame frame_;
  eprosima::fastdds::dds::DomainParticipant* participant_;
  eprosima::fastdds::dds::Publisher* publisher_;
  eprosima::fastdds::dds::Topic* topic_;
  eprosima::fastdds::dds::DataWriter* writer_;
  eprosima::fastdds::dds::TypeSupport type_;
};
