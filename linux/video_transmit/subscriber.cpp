#include <thread>
#include <vector>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>

#include "subscriber.h"


using namespace eprosima::fastdds::dds;

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

  topic_ = participant_->create_topic("VedioFrame", "video::Frame", TOPIC_QOS_DEFAULT);
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
  cv::Mat img = cv::imdecode(cv::Mat(frame_.frame_bytes()), CV_LOAD_IMAGE_COLOR); // decode
  cv::imshow("image", img);
  cv::waitKey(30);

  // image write
  char name[64];
  sprintf(name, "decode_image.ipg");
  std::vector<int> quality;
  quality[0] = cv::IMWRITE_JPEG_QUALITY;
  quality[1] = 50;
  imwrite(name, img, quality);
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