from server import HomeRPC

if __name__ == '__main__':
    # 启动HomeRPC
    HomeRPC.setup(ip = "192.168.43.9", log = True)

    # 等待ESP32连接
    input("Waiting for ESP32 to connect...")
    
    place = HomeRPC.place("room")
    # 调用ESP32客户端服务
    place.device("light").id(1).call("trigger", 1, timeout_s = 10)
    print("led status: ", place.device("light").id(1).call("status", timeout_s = 10))