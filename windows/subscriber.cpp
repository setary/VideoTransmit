#include <thread>
#include <vector>

#include "client.h"
#include "subscriber.h"

uint64_t count = 0;
void on_data_available(dds_entity_t reader, void* arg)
{
    VideoSubscriber* self = (VideoSubscriber*)arg;
    dds_sample_info_t infos[1];
    dds_return_t rc;


    rc = dds_take(reader, self->samples, infos, 1, 1);
    if (rc < 0) {
        printf("Take failed: %s\n", dds_strretcode(-rc));
    }
    else if ((rc > 0) && (infos[0].valid_data)) {
        self->frame_ = (video_Frame*)self->samples[0];
        self->decode();
    }
}


VideoSubscriber::VideoSubscriber() {
    samples[0] = video_Frame__alloc();
}

VideoSubscriber::~VideoSubscriber() {
  disable();
  video_Frame_free(samples[0], DDS_FREE_ALL);
}

bool VideoSubscriber::enable() {
    dig_hole("sub_data");
    dig_hole("sub_meta");
    std::string pub_data_ip;
    int pub_data_port = 0;
    while (!get_address_by_name("pub_data", pub_data_ip, pub_data_port)) {
      printf("get peer addr failed, try again.\n");
      dds_sleepfor(DDS_SECS (1));
      dig_hole("sub_data");
      dig_hole("sub_meta");
    }
    printf("get pub_data ip: %s, port: %d\n", pub_data_ip.c_str(), pub_data_port);

    std::string pub_meta_ip;
    int pub_meta_port = 0;
    while (!get_address_by_name("pub_meta", pub_meta_ip, pub_meta_port)) {
      printf("get peer addr failed, try again.\n");
      dds_sleepfor(DDS_SECS (1));
      dig_hole("sub_data");
      dig_hole("sub_meta");
    }
    printf("get pub_meta ip: %s, port: %d\n", pub_meta_ip.c_str(), pub_meta_port);

    std::string sub_data_ip;
    int sub_data_port = 0;
    while (!get_address_by_name("sub_data", sub_data_ip, sub_data_port)) {
      printf("get peer addr failed, try again.\n");
      dds_sleepfor(DDS_SECS (1));
      dig_hole("sub_data");
      dig_hole("sub_meta");
    }
    printf("get sub_data ip: %s, port: %d\n", sub_data_ip.c_str(), sub_data_port);

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
        "</Domain></CycloneDDS>", pub_data_ip.c_str(), pub_data_port);
    domain_ = dds_create_domain(DDS_DOMAIN_DEFAULT, config);
  participant_ = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant_ < 0) {
    printf("dds_create_participant: %s\n", dds_strretcode(-participant_));
    return false;
  }

  topic_ = dds_create_topic(participant_, &video_Frame_desc, "video_Frame", NULL, NULL);
  if (topic_ < 0) {
    printf("dds_create_topic: %s\n", dds_strretcode(-topic_));
    return false;
  }

  dds_qos_t *qos = dds_create_qos();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));

  /* 创建监听器并设置回调 */
  dds_listener_t* listener = dds_create_listener(NULL);
  dds_lset_data_available_arg(listener, on_data_available, (void*)this, true);
  //dds_set_listener(reader_, listener);
  printf("in this: %p\n", this);

  reader_ = dds_create_reader (participant_, topic_, qos, listener);
  if (reader_ < 0) {
    dds_delete_qos(qos);
    printf("dds_create_reader: %s\n", dds_strretcode(-reader_));
    return false;
  }
  dds_delete_qos(qos);

  return true;
}

bool VideoSubscriber::disable() {
  fill_all_holes();
  dds_return_t rc = dds_delete (participant_);
  if (rc != DDS_RETCODE_OK) {
    printf("dds_delete: %s\n", dds_strretcode(-rc));
    return false;
  }
  return true;
}

void VideoSubscriber::decode() {
    std::vector<char> frame(frame_->frame_bytes._buffer, frame_->frame_bytes._buffer + frame_->frame_bytes._length);
    cv::Mat img = cv::imdecode(cv::Mat(frame), CV_LOAD_IMAGE_COLOR); // decode
    cv::imshow("image", img);
    cv::waitKey(30);
}

void VideoSubscriber::run() {
  /*void *samples[1];
  samples[0] = video_Frame__alloc();
  dds_sample_info_t infos[1];
  while (true) {
    dds_return_t rc = dds_read(reader_, samples, infos, 1, 1);
    if (rc < 0) {
      printf("dds_read: %s\n", dds_strretcode(-rc));
    }

    if ((rc > 0) && (infos[0].valid_data)) {
        printf("recv a frame\n");
      frame_ = (video_Frame*) samples[0];
      decode();
    } else {
      dds_sleepfor (DDS_MSECS(100));
    }
    video_Frame_free(samples[0], DDS_FREE_ALL);
  }*/

  while (true) {
      dds_sleepfor(DDS_SECS(1));
  };
}