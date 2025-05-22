#include <stdio.h>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastrtps/subscriber/SampleInfo.h>
#include <fastdds/dds/core/status/SubscriptionMatchedStatus.hpp>

#include "videoPubSubTypes.h"


class VideoSubscriber {
public:
  VideoSubscriber();
  ~VideoSubscriber();

  bool enable();

  bool disable();

  void run();

private:

  eprosima::fastdds::dds::DomainParticipant* participant_;
  eprosima::fastdds::dds::Subscriber* subscriber_;
  eprosima::fastdds::dds::Topic* topic_;
  eprosima::fastdds::dds::DataReader* reader_;
  eprosima::fastdds::dds::TypeSupport type_;

  class SubListener : public eprosima::fastdds::dds::DataReaderListener {
    public:
      SubListener() {
        frame_.frame_id(0);
      }
      ~SubListener() override {}

      void on_data_available(eprosima::fastdds::dds::DataReader* reader) override;

      void decode();
    
    private:
      video::Frame frame_;
      std::vector<uint8_t> jpeg_data_;
  } listener_;
};
