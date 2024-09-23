import socket

import sys
import importlib.util
from concurrent.futures import ThreadPoolExecutor

vasp = None
lumapi = None
scriptPath = None
filePath = None
simulationTask = None
logFile = None
executor = ThreadPoolExecutor(max_workers=100)

token = 0

def sendMessageToFUIDSL(fuldsisConnect, message):
    if token != 0:
        token_byte = token.to_bytes(4, byteorder='little', signed=True)
        message_len = len(message)
        message_len_byte = message_len.to_bytes(4, byteorder='little', signed=True)
        message_byte = message.encode('utf-8')  # 使用UTF-8编码
        message_all = token_byte + message_len_byte + message_byte
        fuldsisConnect.sendall(message_all)

def parseFromFULDSIMessage(data):
    f_token = data[:4]
    mess_len = data[4:8]
    token_ = int.from_bytes(f_token, byteorder='little')
    message_len_ = int.from_bytes(mess_len, byteorder='little')
    message_bytes = data[8:(8+message_len_)]
    message = message_bytes.decode()
    return message

def startVASP():
    global vasp
    global lumapi
    global logFile
    global scriptPath
    global filePath
    if lumapi == None:
        sys.path.append(scriptPath)
        spec_win = importlib.util.spec_from_file_location("lumapi", scriptPath + "lumapi.py")
        lumapi = importlib.util.module_from_spec(spec_win)
        spec_win.loader.exec_module(lumapi)
    if vasp == None:
        vasp = lumapi.VASP()
    vasp.load(filePath)
    # logFile = filePath.replace(".fsp", "_p0.log") # 由于VASP没有log文件，所以这里不需要

def runSimulationVASP():
    global simulationTask
    if vasp != None:
        simulationTask = executor.submit(lambda: vasp.run())

def isSimulationDone(key):
    return simulationTask.done()

def getLogFilePath():
    return logFile

def visualVASPInner():
    if vasp != None:
        E = vasp.getresult("xy", "E")
        vasp.visualize(E)

def visualVASP():
    executor.submit(visualVASPInner)


def create_tcp_connection(ip, port):
    # 创建一个新的socket对象
    fuldsisConnect = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # 连接到指定的IP和端口
    fuldsisConnect.connect((ip, port))
    sendMessageToFUIDSL(fuldsisConnect,'Connection successful.')
    startVASP()
    print("Connected to {}:{}".format(ip, port))
    return fuldsisConnect

def listen_for_data(fuldsisConnect):
    while True:
        # 接收数据（最多1024字节）
        data = fuldsisConnect.recv(1024)
        if not data:
            # 如果没有收到数据，说明连接已经关闭
            break
        print("Received data:", data.decode())
        cmd = parseFromFULDSIMessage(data)
        if cmd == 'Compute':
            path = "VASP Compute " + getLogFilePath()
            sendMessageToFUIDSL(fuldsisConnect, path)
            vasp.run()
            sendMessageToFUIDSL(fuldsisConnect,'Compute Done.')
        if cmd == 'ShowResultView':
            visualVASPInner()

if __name__ == "__main__":
    scriptPath = sys.argv[1]
    filePath = sys.argv[2]
    ipandPort = sys.argv[3].split(":")
    ip = ipandPort[0]
    port = ipandPort[1]
    token = int(sys.argv[4])
    # 请替换为你要连接的端口号
    fuldsisConnect = create_tcp_connection(ip, int(port))
    listen_for_data(fuldsisConnect)
    fuldsisConnect.close()