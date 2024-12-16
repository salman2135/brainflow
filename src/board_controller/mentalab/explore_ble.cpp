#include "explore_ble.h"
#include "custom_cast.h"
#include "timestamp.h"
#include <string.h>

#define ExplorePro_WRITE_CHAR  "fffe0002-b5a3-f393-e0a9-e50e24dcca9e"
#define ExplorePro_NOTIFY_CHAR "fffe0003-b5a3-f393-e0a9-e50e24dcca9e"


static void ExplorePro_adapter_1_on_scan_found (
    simpleble_adapter_t adapter, simpleble_peripheral_t peripheral, void *board)
{
    ((ExplorePro *)(board))->adapter_1_on_scan_found (adapter, peripheral);
}

static void ExplorePro_read_notifications (simpleble_uuid_t service,
    simpleble_uuid_t characteristic, uint8_t *data, size_t size, void *board)
{
    
        ((ExplorePro *)(board))->read_data (service, characteristic, data, size, 0);
}

ExplorePro::ExplorePro (int board_id, struct BrainFlowInputParams params)
    : BLELibBoard ((int)BoardIds::EXPLORE_PRO_32_CHAN_BOARD, params)
{
}

ExplorePro::~ExplorePro ()
{
    skip_logs = true;
    release_session ();
}

int ExplorePro::prepare_session ()
{
    if (initialized)
    {
        safe_logger (spdlog::level::info, "Session is already prepared");
        return (int)BrainFlowExitCodes::STATUS_OK;
    }
    if (params.timeout < 1)
    {
        params.timeout = 5;
    }
    safe_logger (spdlog::level::info, "Use timeout for discovery: {}", params.timeout);
    if (!init_dll_loader ())
    {
        safe_logger (spdlog::level::err, "Failed to init dll_loader");
        return (int)BrainFlowExitCodes::GENERAL_ERROR;
    }
    size_t adapter_count = simpleble_adapter_get_count ();
    if (adapter_count == 0)
    {
        safe_logger (spdlog::level::err, "No BLE adapters found");
        return (int)BrainFlowExitCodes::UNABLE_TO_OPEN_PORT_ERROR;
    }

    explore_pro_adapter = simpleble_adapter_get_handle (0);
    if (explore_pro_adapter == NULL)
    {
        safe_logger (spdlog::level::err, "Adapter is NULL");
        return (int)BrainFlowExitCodes::UNABLE_TO_OPEN_PORT_ERROR;
    }

    simpleble_adapter_set_callback_on_scan_found (
        explore_pro_adapter, ::ExplorePro_adapter_1_on_scan_found, (void *)this);

    if (!simpleble_adapter_is_bluetooth_enabled ())
    {
        safe_logger (spdlog::level::warn, "Probably bluetooth is disabled.");
        // dont throw an exception because of this
        // https://github.com/OpenBluetoothToolbox/SimpleBLE/issues/115
    }

    simpleble_adapter_scan_start (explore_pro_adapter);
    int res = (int)BrainFlowExitCodes::STATUS_OK;
    std::unique_lock<std::mutex> lk (m);
    auto sec = std::chrono::seconds (1);
    if (cv.wait_for (
            lk, params.timeout * sec, [this] { return this->explore_pro_peripheral != NULL; }))
    {
        safe_logger (spdlog::level::info, "Found ExplorePro device");
    }
    else
    {
        safe_logger (spdlog::level::err, "Failed to find ExplorePro Device");
        res = (int)BrainFlowExitCodes::BOARD_NOT_READY_ERROR;
    }
    simpleble_adapter_scan_stop (explore_pro_adapter);
    if (res == (int)BrainFlowExitCodes::STATUS_OK)
    {
        // for safety
        for (int i = 0; i < 3; i++)
        {
            if (simpleble_peripheral_connect (explore_pro_peripheral) == SIMPLEBLE_SUCCESS)
            {
                safe_logger (spdlog::level::info, "Connected to ExplorePro Device");
                res = (int)BrainFlowExitCodes::STATUS_OK;

                break;
            }
            else
            {
                safe_logger (
                    spdlog::level::warn, "Failed to connect to ExplorePro Device: {}/3", i);
                res = (int)BrainFlowExitCodes::BOARD_NOT_READY_ERROR;
#ifdef _WIN32
                Sleep (1000);
#else
                sleep (1);
#endif
            }
        }
    }
    else
    {
// https://github.com/OpenBluetoothToolbox/SimpleBLE/issues/26#issuecomment-955606799
#ifdef __linux__
        usleep (1000000);
#endif
    }

    bool control_characteristics_found = false;

    if (res == (int)BrainFlowExitCodes::STATUS_OK)
    {
        size_t services_count = simpleble_peripheral_services_count (explore_pro_peripheral);
        for (size_t i = 0; i < services_count; i++)
        {
            simpleble_service_t service;
            if (simpleble_peripheral_services_get (explore_pro_peripheral, i, &service) !=
                SIMPLEBLE_SUCCESS)
            {
                safe_logger (spdlog::level::err, "failed to get service");
                res = (int)BrainFlowExitCodes::BOARD_NOT_READY_ERROR;
            }

            safe_logger (spdlog::level::trace, "found servce {}", service.uuid.value);
            for (size_t j = 0; j < service.characteristic_count; j++)
            {
                safe_logger (spdlog::level::trace, "found characteristic {}",
                    service.characteristics[j].uuid.value);

                if (strcmp (service.characteristics[j].uuid.value,
                        ExplorePro_WRITE_CHAR) == 0) // Write Characteristics
                {
                    write_characteristics = std::pair<simpleble_uuid_t, simpleble_uuid_t> (
                        service.uuid, service.characteristics[j].uuid);
                    control_characteristics_found = true;
                    safe_logger (spdlog::level::info, "found control characteristic");
                }
                if (strcmp (service.characteristics[j].uuid.value,
                        ExplorePro_NOTIFY_CHAR) == 0) // Notification Characteristics
                {   
                    if (simpleble_peripheral_notify (explore_pro_peripheral, service.uuid,
                            service.characteristics[j].uuid, ::ExplorePro_read_notifications,
                            (void *)this) == SIMPLEBLE_SUCCESS)
                    {
                        initialized = true;
                        notified_characteristics = std::pair<simpleble_uuid_t, simpleble_uuid_t> (
                            service.uuid, service.characteristics[j].uuid);
                            return (int)BrainFlowExitCodes::STATUS_OK;
                    }
                    else
                    {
                        safe_logger (spdlog::level::err, "Failed to notify for {} {}",
                            service.uuid.value, service.characteristics[j].uuid.value);
                        res = (int)BrainFlowExitCodes::GENERAL_ERROR;
                    }
                }
            }
        }
    }

    if ((res == (int)BrainFlowExitCodes::STATUS_OK) && (control_characteristics_found))
    {
        return res;
    }
    else
    {
        release_session ();
    }
}

int ExplorePro::start_stream (int buffer_size, const char *streamer_params)
{
    if (!initialized)
    {
        return (int)BrainFlowExitCodes::BOARD_NOT_CREATED_ERROR;
    }
    int res = prepare_for_acquisition (buffer_size, streamer_params);
    if (res == (int)BrainFlowExitCodes::STATUS_OK)
    {
        safe_logger (spdlog::level::debug, "Start command Send 250sps");
        is_streaming = true;
    }

    return res;
}

int ExplorePro::stop_stream ()
{
    if (explore_pro_peripheral == NULL)
    {
        return (int)BrainFlowExitCodes::BOARD_NOT_CREATED_ERROR;
    }
    int res = (int)BrainFlowExitCodes::STATUS_OK;
    is_streaming = false;
    return res;
}

int ExplorePro::release_session ()
{
    if (initialized)
    {
        // repeat it multiple times, failure here may lead to a crash
        for (int i = 0; i < 2; i++)
        {
            stop_stream ();
            // need to wait for notifications to stop triggered before unsubscribing, otherwise
            // macos fails inside simpleble with timeout
#ifdef _WIN32
            Sleep (2000);
#else
            sleep (2);
#endif
            if (simpleble_peripheral_unsubscribe (explore_pro_peripheral,
                    notified_characteristics.first,
                    notified_characteristics.second) != SIMPLEBLE_SUCCESS)
            {
                safe_logger (spdlog::level::err, "failed to unsubscribe for {} {}",
                    notified_characteristics.first.value, notified_characteristics.second.value);
            }
            else
            {
                break;
            }
        }
        free_packages ();
        initialized = false;
    }
    if (explore_pro_peripheral != NULL)
    {
        bool is_connected = false;
        if (simpleble_peripheral_is_connected (explore_pro_peripheral, &is_connected) ==
            SIMPLEBLE_SUCCESS)
        {
            if (is_connected)
            {
                simpleble_peripheral_disconnect (explore_pro_peripheral);
            }
        }
        simpleble_peripheral_release_handle (explore_pro_peripheral);
        explore_pro_peripheral = NULL;
    }
    if (explore_pro_adapter != NULL)
    {
        simpleble_adapter_release_handle (explore_pro_adapter);
        explore_pro_adapter = NULL;
    }

    return (int)BrainFlowExitCodes::STATUS_OK;
}

int ExplorePro::config_board (std::string config, std::string &response)
{
    return config_board (config);
}

int ExplorePro::config_board (std::string config)
{
    if (!initialized)
    {
        return (int)BrainFlowExitCodes::BOARD_NOT_CREATED_ERROR;
    }
    // Calculate the number of bytes
    size_t num_bytes = config.length () / 2; // Each byte is represented by 2 hex characters
    std::vector<uint8_t> byte_array (num_bytes);
    // Convert hex string to byte array
    for (size_t i = 0; i < num_bytes; ++i)
    {
        std::string byte_string = config.substr (i * 2, 2); // Get 2 characters for each byte
        byte_array[i] =
            static_cast<unsigned char> (std::stoul (byte_string, nullptr, 16)); // Convert to byte
    }

    if (simpleble_peripheral_write_command (explore_pro_peripheral, write_characteristics.first,
            write_characteristics.second, byte_array.data (),
            sizeof (byte_array)) != SIMPLEBLE_SUCCESS)
    {
        safe_logger (spdlog::level::err, "failed to send command {} to device", config.c_str ());
        return (int)BrainFlowExitCodes::BOARD_WRITE_ERROR;
    }
    return (int)BrainFlowExitCodes::STATUS_OK;
}

void ExplorePro::adapter_1_on_scan_found (
    simpleble_adapter_t adapter, simpleble_peripheral_t peripheral)
{
    char *peripheral_identified = simpleble_peripheral_identifier (peripheral);
    char *peripheral_address = simpleble_peripheral_address (peripheral);
    bool found = false;
    if (!params.mac_address.empty ())
    {
        if (strcmp (peripheral_address, params.mac_address.c_str ()) == 0)
        {
            found = true;
        }
    }
    else
    {
        if (!params.serial_number.empty ())
        {
            if (strcmp (peripheral_identified, params.serial_number.c_str ()) == 0)
            {
                found = true;
            }
        }
        else
        {
            if (strncmp (peripheral_identified, "Explore_", 7) == 0)
            {
                found = true;
            }
        }
    }

    safe_logger (spdlog::level::trace, "address {}", peripheral_address);
    simpleble_free (peripheral_address);
    safe_logger (spdlog::level::trace, "identifier {}", peripheral_identified);
    simpleble_free (peripheral_identified);

    if (found)
    {
        {
            std::lock_guard<std::mutex> lk (m);
            explore_pro_peripheral = peripheral;
        }
        cv.notify_one ();
    }
    else
    {
        simpleble_peripheral_release_handle (peripheral);
    }
}

void ExplorePro::read_data (simpleble_uuid_t service, simpleble_uuid_t characteristic,
    uint8_t *data, size_t size, int channel_num)
{
    safe_logger (spdlog::level::debug, "Got data from device!");
    int num_rows = board_descr["default"]["num_rows"];
    double *package = new double[num_rows];
    for (int i = 0; i < num_rows; i++)
    {
        package[i] = 0.0;
    }
    
    std::vector<int> eeg_channels = board_descr["default"]["eeg_channels"];

    for(int i=0;i<32;i++)
    {
        package[eeg_channels[i]] = 400000;
    }
    package[board_descr["default"]["timestamp_channel"].get<int> ()] = get_timestamp ();
    push_package (&package[0]);
}