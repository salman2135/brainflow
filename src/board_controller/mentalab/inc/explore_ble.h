#include <condition_variable>
#include <mutex>
#include <thread>

#include "ble_lib_board.h"
#include "board.h"
#include "board_controller.h"

class ExplorePro : public BLELibBoard
{

private:
    void parseBuffer();
    uint16_t extractUint16(const uint8_t* buffer);
    uint32_t extractUint32(const uint8_t* buffer);
    void processPacket(const uint8_t* packet);
public:
    ExplorePro (int board_id, struct BrainFlowInputParams params);
    ~ExplorePro ();

    void adapter_1_on_scan_found (simpleble_adapter_t adapter, simpleble_peripheral_t peripheral);
    int prepare_session ();
    int config_board (std::string config, std::string &response);
    int config_board (std::string config);
    int start_stream (int buffer_size, const char *streamer_params);
    int stop_stream ();
    int release_session ();
    void read_data (simpleble_uuid_t service, simpleble_uuid_t characteristic, uint8_t *data,
        size_t size, int channel_num);


protected:
    bool initialized;
    std::mutex m;
    std::condition_variable cv;
    bool is_streaming;
    volatile simpleble_peripheral_t explore_pro_peripheral;
    volatile simpleble_adapter_t explore_pro_adapter;
    std::pair<simpleble_uuid_t, simpleble_uuid_t> write_characteristics;
    std::pair<simpleble_uuid_t, simpleble_uuid_t> notified_characteristics;
    uint8_t sharedBuffer[1024];
    size_t bufferLength = 0;
    size_t MAX_BUFFER_SIZE = 1024;
    size_t HEADER_SIZE = 8;
};