#include <thread>
#include <vector>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>

#include "subscriber.h"


using namespace eprosima::fastdds::dds;


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


VideoSubscriber::VideoSubscriber()
  : participant_(nullptr)
  , subscriber_(nullptr)
  , topic_(nullptr)
  , reader_(nullptr)
  , type_(new video::FramePubSubType()) {

}

VideoSubscriber::~VideoSubscriber() {
  disable();
}

bool VideoSubscriber::enable() {
  auto factory = DomainParticipantFactory::get_instance();

  participant_ = factory->create_participant(0, PARTICIPANT_QOS_DEFAULT);
  if (participant_ == nullptr) {
    printf("create participant failed.\n");
    return false;
  }

  type_.register_type(participant_);

  subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr);
  if (subscriber_ == nullptr) {
    printf("create subscriber failed.\n");
    return false;
  }

  topic_ = participant_->create_topic("VideoFrame", "video::Frame", TOPIC_QOS_DEFAULT);
  if (topic_ == nullptr) {
    printf("create topic failed.\n");
    return false;
  }

  reader_ = subscriber_->create_datareader(topic_, DATAREADER_QOS_DEFAULT, &listener_);
  if (reader_ == nullptr) {
    printf("create datareader failed.\n");
    return false;
  }
  return true;
}

bool VideoSubscriber::disable() {
  if (reader_ != nullptr) {
    subscriber_->delete_datareader(reader_);
  }

  if (topic_ != nullptr) {
    participant_->delete_topic(topic_);
  }

  if (subscriber_ != nullptr) {
    participant_->delete_subscriber(subscriber_);
  }

  DomainParticipantFactory::get_instance()->delete_participant(participant_);
}

void VideoSubscriber::SubListener::decode() {
  const int JPEG_HDR_SIZE = sizeof(JPEGHeader);
  const int RTP_HDR_SIZE = sizeof(RTPHeader);
  uint32_t total_size = frame_.frame_bytes().size();

  // 解析rtp头
  RTPHeader* rtp_hdr = (RTPHeader*)frame_.frame_bytes().data();
  if (rtp_hdr->payload_type != 26) {
    printf("encode type is not jpeg, drop it!\n");
    return;
  }

  int originLen = jpeg_data_.size();
  int curLen = total_size - RTP_HDR_SIZE - JPEG_HDR_SIZE;
  jpeg_data_.resize(originLen + curLen);
  memcpy(jpeg_data_.data() + originLen, frame_.frame_bytes().data() + RTP_HDR_SIZE + JPEG_HDR_SIZE, curLen);

  // 最后一包，显示图片
  if (rtp_hdr->marker) {
    cv::Mat img = cv::imdecode(cv::Mat(jpeg_data_), CV_LOAD_IMAGE_COLOR); // decode
    cv::imshow("image", img);
    cv::waitKey(30);
    jpeg_data_.resize(0);

    // image write
    char name[64];
    sprintf(name, "decode_image.jpg");
    std::vector<int> quality;
    quality[0] = cv::IMWRITE_JPEG_QUALITY;
    quality[1] = 50;
    imwrite(name, img, quality);
  }
}

void VideoSubscriber::SubListener::on_data_available(DataReader* reader) {
  SampleInfo info;
  if (reader->take_next_sample(&frame_, &info) == ReturnCode_t::RETCODE_OK)
  {
      if (info.instance_state == ALIVE_INSTANCE_STATE)
      {
        printf("receive a frame, frame id: %d\n", frame_.frame_id());
        decode();
      }
  }
}

void VideoSubscriber::run() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}
