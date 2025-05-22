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


struct RTPHeader {
    uint8_t version:2;        // 协议版本（2）
    uint8_t padding:1;        // 塌余填充
    uint8_t extension:1;      // 扩展标识
    uint8_t csrc_count:4;     // 贡献源数量
    uint8_t marker:1;         // 完整帧标记
    uint8_t payload_type:7;   // 负载类型（JPEG=26）
    uint16_t seq_no;          // 网络字节序
    uint32_t timestamp;       // 时间戳（90kHz）
    uint32_t ssrc;            // 同步源标识
} __attribute__((packed));


struct JPEGHeader {
    uint8_t type_specific;    // 固定为0
    uint8_t jpeg_type;        // Baseline=0
    uint8_t q;                // 量化因子（0=默认）
    uint8_t width;            // 原始宽/8（640=80）
    uint8_t height;           // 原始高/8（480=60）
    uint8_t offset[3];        // 分片偏移量（24位）
} __attribute__((packed));


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
