import argparse
import time

from brainflow.board_shim import BoardShim, BrainFlowInputParams, BoardIds

params = BrainFlowInputParams()
params.mac_address = "e8:e1:e9:79:6f:c9"
board = BoardShim(BoardIds.EXPLORE_PRO_32_CHAN_BOARD, params)
board.prepare_session()
board.start_stream ()
time.sleep(10)
# data = board.get_current_board_data (256) # get latest 256 packages or less, doesnt remove them from internal buffer
data = board.get_board_data()  # get all data and remove it from internal buffer
print(data)
board.stop_stream()
board.release_session()