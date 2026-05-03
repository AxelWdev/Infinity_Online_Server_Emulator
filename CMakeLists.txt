cmake_minimum_required(VERSION 3.20)

project(CPP_Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(cpp_server_lib
    src/core/AccountDatabase.cpp
    src/core/GameDataCatalog.cpp
    src/game/InventoryService.cpp
    src/core/Utf.cpp
    src/core/LzssCodec.cpp
    src/core/Json.cpp
    src/core/Logger.cpp
    src/core/OptionsStore.cpp
    src/core/PacketLog.cpp
    src/server/ClientSession.cpp
    src/server/OptionPacketHelpers.cpp
    src/server/PacketScheduler.cpp
    src/server/RoomRegistry.cpp
    src/server/RoomStatePackets.cpp
    src/server/SessionPacketEffects.cpp
    src/server/TcpAccountHandlers.cpp
    src/server/TcpAdminCommands.cpp
    src/server/TcpAuthHandlers.cpp
    src/server/TcpAutoHandlers.cpp
    src/server/TcpConnectionLoop.cpp
    src/server/TcpEquipmentHandlers.cpp
    src/server/TcpFullRoomStateHandlers.cpp
    src/server/TcpItemRemovalHandlers.cpp
    src/server/TcpListHandlers.cpp
    src/server/TcpNicknameHandlers.cpp
    src/server/TcpPacketDispatch.cpp
    src/server/TcpQuickslotHandlers.cpp
    src/server/TcpRoomCharacterSelectHandlers.cpp
    src/server/TcpRoomCreateHandlers.cpp
    src/server/TcpRoomInfoHandlers.cpp
    src/server/TcpRoomLeaveHandlers.cpp
    src/server/TcpRoomPlayerState.cpp
    src/server/TcpRoomStartHandlers.cpp
    src/server/TcpRoomState.cpp
    src/server/TcpRuntimeRoomState.cpp
    src/server/TcpSessionOptions.cpp
    src/server/TcpSessionSupport.cpp
    src/server/TcpSkillShopHandlers.cpp
    src/server/TcpStateHandlers.cpp
    src/server/TcpShopHandlers.cpp
    src/server/TcpUdpBridge.cpp
    src/server/TcpLzssServer.cpp
    src/udp/UdpCombat.cpp
    src/udp/UdpGameServer.cpp
    src/udp/UdpGamePackets.cpp
    src/udp/UdpProtocol.cpp
)

target_include_directories(cpp_server_lib PUBLIC include)

if(WIN32)
    target_link_libraries(cpp_server_lib PUBLIC ws2_32)
endif()

add_executable(tcp_lzss_server_cpp src/main.cpp)
target_link_libraries(tcp_lzss_server_cpp PRIVATE cpp_server_lib)

add_executable(udp_traffic_logger src/tools/udp_traffic_logger.cpp)
target_link_libraries(udp_traffic_logger PRIVATE cpp_server_lib)

add_executable(udp_hook_log_decode src/tools/udp_hook_log_decode.cpp)
target_link_libraries(udp_hook_log_decode PRIVATE cpp_server_lib)
