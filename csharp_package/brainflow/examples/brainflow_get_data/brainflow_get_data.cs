
using System;

using brainflow;
using brainflow.math;
using static System.Runtime.InteropServices.JavaScript.JSType;


namespace examples
{
    class GetBoardData
    {
        static void Main (string[] args)
        {
            BoardShim.enable_dev_board_logger ();

            BrainFlowInputParams input_params = new BrainFlowInputParams ();
            input_params.mac_address = "e8:e1:e9:79:6f:c9";
            //input_params.mac_address = "d2:64:ca:33:ed:c0";
            int board_id = 58;

            BoardShim board_shim = new BoardShim (board_id, input_params);
            board_shim.prepare_session ();
            board_shim.start_stream ();
            System.Threading.Thread.Sleep (5000);
            board_shim.stop_stream ();
            double[,] unprocessed_data = board_shim.get_current_board_data (500, (int)BrainFlowPresets.AUXILIARY_PRESET);
            DataFilter.write_file (unprocessed_data, "test.csv", "w");
            board_shim.release_session ();

        }

        static int parse_args (string[] args, BrainFlowInputParams input_params)
        {
            int board_id = (int)BoardIds.SYNTHETIC_BOARD; //assume synthetic board by default
            // use docs to get params for your specific board, e.g. set serial_port for Cyton
            for (int i = 0; i < args.Length; i++)
            {
                if (args[i].Equals ("--ip-address"))
                {
                    input_params.ip_address = args[i + 1];
                }
                if (args[i].Equals ("--mac-address"))
                {
                    input_params.mac_address = args[i + 1];
                }
                if (args[i].Equals ("--serial-port"))
                {
                    input_params.serial_port = args[i + 1];
                }
                if (args[i].Equals ("--other-info"))
                {
                    input_params.other_info = args[i + 1];
                }
                if (args[i].Equals ("--ip-port"))
                {
                    input_params.ip_port = Convert.ToInt32 (args[i + 1]);
                }
                if (args[i].Equals ("--ip-protocol"))
                {
                    input_params.ip_protocol = Convert.ToInt32 (args[i + 1]);
                }
                if (args[i].Equals ("--board-id"))
                {
                    board_id = Convert.ToInt32 (args[i + 1]);
                }
                if (args[i].Equals ("--timeout"))
                {
                    input_params.timeout = Convert.ToInt32 (args[i + 1]);
                }
                if (args[i].Equals ("--serial-number"))
                {
                    input_params.serial_number = args[i + 1];
                }
                if (args[i].Equals ("--file"))
                {
                    input_params.file = args[i + 1];
                }
            }
            return board_id;
        }
    }
}
