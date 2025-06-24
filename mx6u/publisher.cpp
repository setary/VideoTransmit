#include <pthread.h>
#include <vector>
#include <arpa/inet.h>
#include <algorithm>

#include "client.h"
#include "publisher.h"


static const int MTU = 1400;
static const uint32_t TS_INC = 90000 / 30; // 3000 (90kHz时钟)


VideoPublisher::VideoPublisher() {
}

VideoPublisher::~VideoPublisher() {
  disable();
}

bool VideoPublisher::enable(int camera_id) {
  printf("camera id: %d\n", camera_id);
  cap_.open(camera_id);
  if (!cap_.isOpened()) {
    printf("unable to open the camera.\n");
    return false;
  }
  printf("open camera success.\n");

  std::string ip;
  int port = 0;
  while (!getPeerAddr("publisher", ip, port)) {
    printf("get peer addr failed, try again.\n");
    dds_sleepfor(DDS_SECS (1));

  }
  char config[1024];
  memset(config, 0, sizeof(config));
  sprintf(config, "<CycloneDDS><Domain Id=\"any\">"
      "<General>"
      "<AllowMulticast>false</AllowMulticast>"
      "<MaxMessageSize>65500B</MaxMessageSize>"
      "</General>"
      "<Discovery>"
      "<ParticipantIndex>0</ParticipantIndex>"
      "<Peers> < Peer Address = \"%s:%d\" / > < / Peers>"
      "</Discovery>"
      "</Domain></CycloneDDS>", ip.c_str(), port);
  domain_ = dds_create_domain(DDS_DOMAIN_DEFAULT, config);

  participant_ = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant_ < 0) {
    printf("dds_create_participant: %s\n", dds_strretcode(-participant_));
    return false;
  }

  topic_ = dds_create_topic(participant_, &video_Frame_desc, "video_Frame", NULL, NULL);
  if (topic_ < 0) {
    printf("dds_create_topic: %s\n", dds_strretcode(-topic_));
    return false;
  }

  writer_ = dds_create_writer(participant_, topic_, NULL, NULL);
  if (writer_ < 0) {
    printf("dds_create_writer: %s\n", dds_strretcode(-writer_));
    return false;
  }
  printf("enable success.\n");

  return true;
}

bool VideoPublisher::disable() {
  dds_return_t status = dds_delete (participant_);
  if (status < 0)
    printf("dds_delete: %s\n", dds_strretcode(-status));

  printf("release the camera capture.\n");
  cap_.release();
  return true;
}

bool VideoPublisher::capture() {
  bool ret = cap_.read(image_);
  if (!ret || image_.empty()) {
    printf("no frame has been grabbed, camera is disconnected or there is no frame.\n");
    return false;
  }
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

bool VideoPublisher::publish(uint8_t* data, int dataLen) {
  frame_.frame_bytes._buffer = (char*)dds_alloc (dataLen);
  frame_.frame_bytes._length = dataLen;
  frame_.frame_bytes._release = true;
  for (int i = 0; i < dataLen; i++) {
    frame_.frame_bytes._buffer[i] = data[i];
  }

  dds_return_t status = dds_write(writer_, &frame_);
  if (status != DDS_RETCODE_OK) {
    printf("dds_write: %s\n", dds_strretcode(-status));
  } else {
    frame_.frame_id++;
  }
  dds_free (frame_.frame_bytes._buffer);
  return true;
}

void* VideoPublisher::runThread(void* arg) {
  VideoPublisher* self = static_cast<VideoPublisher*>(arg);
  uint32_t status;
  dds_return_t rc = dds_set_status_mask(self->writer_, DDS_PUBLICATION_MATCHED_STATUS);
  if (rc != DDS_RETCODE_OK) {
    printf("dds_set_status_mask: %s\n", dds_strretcode(-rc));
    return NULL;
  }

  while(!(status & DDS_PUBLICATION_MATCHED_STATUS))
  {
    rc = dds_get_status_changes (self->writer_, &status);
    if (rc != DDS_RETCODE_OK) {
      printf("dds_get_status_changes: %s\n", dds_strretcode(-rc));
      return NULL;
    }
    dds_sleepfor(DDS_MSECS (100));
  }

  printf("subscriber is online\n");
  while (true) {
    if (!self->capture()) {
      printf("grab image failed.\n");
      dds_sleepfor(DDS_SECS(1));
      continue;
    }

    if (!self->encode()) {
      printf("encode frame failed, drop this frame.\n");
      continue;
    }

    self->publish(self->jpeg_data_.data(), self->jpeg_data_.size());
    dds_sleepfor(DDS_MSECS (1));
  }
  return NULL;
}

void VideoPublisher::run() {
  pthread_t t;
  int ret = pthread_create(&t, NULL, VideoPublisher::runThread, this);
  if (ret) {
    printf("pthread_create failed, ret: %d\n", ret);
    return;
  }
  pthread_join(t, NULL);
}
