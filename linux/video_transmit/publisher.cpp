#include <thread>
#include <vector>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>

#include "publisher.h"

using namespace eprosima::fastdds::dds;

static int deviceID = 0;          // 0 = open default camera
static int apiID = cv::CAP_ANY;   // 0 = autodetect default API


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

  frame_.frame_id(0);

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
  bool ret = cap_.read(image_);
  if (!ret || image_.empty()) {
    printf("no frame has been grabbed, camera is disconnected or there is no frame.\n");
    return false;
  }
  return true;
}

void VideoPublisher::compress() {
  std::vector<uint8_t> img_encode;
  std::vector<int> quality;
  quality.push_back(cv::IMWRITE_JPEG_QUALITY);
  quality.push_back(50);  // compression ratio is 50%
  cv::imencode(".jpg", image_, img_encode, quality);

  std::vector<char> img_cp;
  for(int i = 0; i < img_encode.size(); i++) {
    img_cp[i] = img_encode[i];
  }
  frame_.frame_bytes(img_cp);
}

void VideoPublisher::encode() {

}

bool VideoPublisher::publish() {
  frame_.frame_id(frame_.frame_id() + 1);
  writer_->write(&frame_);
  return true;
}

void VideoPublisher::runThread() {
  while (true) {
    if (!capture()) {
      printf("grab image failed.\n");
      break;
    }

    compress();

    encode();

    publish();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}

void VideoPublisher::run() {
  std::thread t(&VideoPublisher::runThread, this);
  t.join();
}