# Tests for the DUNE-DAQ -> WCT bridge (ToFrame), moved here from dune-daq-wct.
# daq_to_frame PUBLIC-propagates WireCell + dune_daq_codec, so linking it is enough.
add_executable(test_to_frame test_to_frame.cpp)
target_link_libraries(test_to_frame PRIVATE daq_to_frame ${DDA_RUNTIME_LIBS})
add_test(NAME to_frame COMMAND test_to_frame)

# File-driven ChannelMap via OnlineOfflineChannels (ddm-3j8.1.13). Reference-free.
add_executable(test_file_channel_map test_file_channel_map.cpp)
target_link_libraries(test_file_channel_map PRIVATE daq_to_frame ${DDA_RUNTIME_LIBS})
add_test(NAME file_channel_map COMMAND test_file_channel_map)
