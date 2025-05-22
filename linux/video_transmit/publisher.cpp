#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <algorithm>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>

#include "publisher.h"

using namespace eprosima::fastdds::dds;

static int deviceID = 0;          // 0 = open default camera
static int apiID = cv::CAP_ANY;   // 0 = autodetect default API

static const int MTU = 1400;
static const uint32_t TS_INC = 90000 / 30; // 3000 (90kHz时钟)


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


VideoPublisher::VideoPublisher()
  : participant_(nullptr)
  , publisher_(nullptr)
  , topic_(nullptr)
  , writer_(nullptr)
  , type_(new video::FramePubSubType()) {

}

VideoPublisher::~VideoPublisher() {
  disable();
}

bool VideoPublisher::enable() {
  cap_.open(deviceID, apiID);
  if (cap_.isOpened()) {
    printf("unable to open the camera.\n");
    return false;
  }
  printf("open camera success.\n");

  cap_.set(cv::CAP_PROP_FRAME_WIDTH, 640);
  cap_.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

  auto factory = DomainParticipantFactory::get_instance();

  participant_ = factory->create_participant(0, PARTICIPANT_QOS_DEFAULT);
  if (participant_ == nullptr) {
    printf("create participant failed.\n");
    return false;
  }

  type_.register_type(participant_);

  publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);
  if (publisher_ == nullptr) {
    printf("create publisher failed.\n");
    return false;
  }

  topic_ = participant_->create_topic("VideoFrame", "video::Frame", TOPIC_QOS_DEFAULT);
  if (topic_ == nullptr) {
    printf("create topic failed.\n");
    return false;
  }

  writer_ = publisher_->create_datawriter(topic_, DATAWRITER_QOS_DEFAULT);
  if (writer_ == nullptr) {
    printf("create datawriter failed.\n");
    return false;
  }

  return true;
}

bool VideoPublisher::disable() {
  if (writer_ != nullptr) {
    publisher_->delete_datawriter(writer_);
  }

  if (publisher_ != nullptr) {
    participant_->delete_publisher(publisher_);
  }

  if (topic_ != nullptr) {
    participant_->delete_topic(topic_);
  }

  DomainParticipantFactory::get_instance()->delete_participant(participant_);

  printf("release the camera capture.\n");
  cap_.release();
}

bool VideoPublisher::capture() {
  cv::Mat frame;
  bool ret = cap_.read(frame);
  if (!ret || frame.empty()) {
    printf("no frame has been grabbed, camera is disconnected or there is no frame.\n");
    return false;
  }
  //cv::cvtColor(frame, image_, cv::COLOR_BGR2YUV_IYUV); // 图像预处理，保持BGR格式
  return true;
}

bool VideoPublisher::encode() {
  std::vector<int> quality;
  quality.push_back(cv::IMWRITE_JPEG_QUALITY);
  quality.push_back(50);  // compression ratio is 50%
  if (!cv::imencode(".jpg", image_, jpeg_data_, quality)) {
    printf("encode jpeg failed.\n");
    return false;
  }
  return true;
}

int VideoPublisher::rtpPack(int offset) {
  const int JPEG_HDR_SIZE = sizeof(JPEGHeader);
  const int RTP_HDR_SIZE = sizeof(RTPHeader);
  uint32_t total_size = jpeg_data_.size();

  int payload_size = std::min(MTU, (int)total_size - offset);
  rtp_pkt_.resize(RTP_HDR_SIZE + JPEG_HDR_SIZE + payload_size);

  // 填充RTP头部
  RTPHeader* rtp_hdr = (RTPHeader*)rtp_pkt_.data();
  memset(rtp_hdr, 0, RTP_HDR_SIZE);
  rtp_hdr->version = 2;
  rtp_hdr->payload_type = 26; // 静态JPEG类型
  rtp_hdr->marker = (offset + payload_size == total_size) ? 1 : 0;
  rtp_hdr->seq_no = htons(seq_no_++);
  rtp_hdr->timestamp = htonl(timestamp_);
  rtp_hdr->ssrc = htonl(0x12345678); // 随机SSRC[citation:12]

  // 填充JPEG头部
  JPEGHeader* jpeg_hdr = (JPEGHeader*)(rtp_pkt_.data() + RTP_HDR_SIZE);
  jpeg_hdr->type_specific = 0;
  jpeg_hdr->jpeg_type = 0;    // Baseline JPEG
  jpeg_hdr->q = 0;            // 默认量化表
  jpeg_hdr->width = 640 / 8;  // 80
  jpeg_hdr->height = 480 / 8; // 60
  uint32_t offset_be = htonl(offset << 8); // 转换为24位
  memcpy(jpeg_hdr->offset, ((uint8_t*)&offset_be) + 1, 3); // 取后3字节

  // 拷贝JPEG数据
  memcpy(rtp_pkt_.data() + RTP_HDR_SIZE + JPEG_HDR_SIZE,
        jpeg_data_.data() + offset, payload_size);

  return offset + payload_size;
}

bool VideoPublisher::publish(uint8_t* data, int dataLen) {
  frame_.frame_id(frame_.frame_id() + 1);
  frame_.frame_bytes(std::vector<char>(data, data + dataLen));
  writer_->write(&frame_);
  return true;
}

void VideoPublisher::runThread() {
  while (true) {
    if (!capture()) {
      printf("grab image failed.\n");
      continue;
    }

    if (!encode()) {
      printf("encode frame failed, drop this frame.\n");
      continue;
    }

    int offset = 0;
    while (offset < jpeg_data_.size()) {
      offset = rtpPack(offset);
      publish(rtp_pkt_.data(), rtp_pkt_.size());
      timestamp_ += TS_INC;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}

void VideoPublisher::run() {
  std::thread t(&VideoPublisher::runThread, this);
  t.join();
}
