import socket
import asyncio
import subprocess
import sys
import importlib.util
import time
from concurrent.futures import ThreadPoolExecutor
import os

from click import command

vasp = None
lumapi = None
filePath = None
simulationTask = None
logFile = None
executor = ThreadPoolExecutor(max_workers=100)

token = 0

import asyncio
import os
import time
import struct

from fdtd import filePath

# TODO: 更改为实际的VASP服务器IP和端口
vasp_host_ip = "192.168.137.2"
vasp_host_port = 12345

material_lib_file = "C:/Users/PS/fuldsi-main/x64/FulDSI/bin/Release/DefaultMaterial/SysMaterial.material"


class RemoteClient:

    def __init__(self, server, port):
        self.server = server
        self.port = port
        self.output_path = ""

    async def connect(self):
        self.reader, self.writer = await asyncio.open_connection(self.server, self.port)

    async def receive_file(self):
        print("Receiving file...")
        # 接收文件名长度（4 字节）
        # 先读第一条 COMPUTE finished \n
        # compute_finished = await self.reader.readuntil(b'\n')
        # print(f'Compute finished: {compute_finished}')
        filename_len_data = await self.reader.readexactly(4)
        if not filename_len_data:
            return False  # 没有接收到数据，结束接收
        filename_len = struct.unpack("I", filename_len_data)[0]

        # 接收文件名
        filename = (await self.reader.readexactly(filename_len)).decode()
        if filename.startswith("END_OF_FILE"):
            return False
        print(f'receive file, filename is {filename}')

        # 接收文件大小（8 字节）
        filesize_data = await self.reader.readexactly(8)
        filesize = struct.unpack("Q", filesize_data)[0]

        # 接收文件内容
        with open(filename, "wb") as f:
            bytes_received = 0
            while bytes_received < filesize:
                chunk_size = min(1024, filesize - bytes_received)
                chunk = await self.reader.readexactly(chunk_size)
                if not chunk:
                    break
                f.write(chunk)
                bytes_received += len(chunk)
            print(f"Received file: {filename} ({filesize} bytes)")
            # 将文件移到output目录下
        os.rename(filename, os.path.join(self.output_path, filename))

        return True

    async def send_file(self, file_path):
        file_name = os.path.basename(file_path)
        file_header = f"FILE{file_name}\n"
        print(f"filename {file_name}\n")
        print(f"filepath {file_path}\n")
        try:
            with open(file_path, "rb") as file:
                # Send file header
                encoded_header = file_header.encode()
                self.writer.write(encoded_header)
                await self.writer.drain()
                print("send file\n")
                # Send file content
                chunk = file.read(1024)
                while chunk:
                    print("write\n")
                    self.writer.write(chunk)
                    await self.writer.drain()
                    chunk = file.read(1024)

            print(f"File {file_name} sent successfully")

            # 发送文件结束标志
            end_signal = "END_OF_FILE\n"
            self.writer.write(end_signal.encode())
            await self.writer.drain()
            print("End of file signal sent")

            # Wait for the response
            print("Waiting for server response...")
            while True:
                if not await self.receive_file():
                    break

        except Exception as e:
            print(f"Error sending file: {e}")
        finally:
            await self.close_connection()

    async def send_command(self, command):
        command_message = f"CMD {command}"
        try:
            self.writer.write(command_message.encode())
            await self.writer.drain()

            # Wait for the response
            response = await self.reader.read(1024)
            print(f"Server response: {response.decode()}")
        except Exception as e:
            print(f"Error sending command: {e}")
        finally:
            await self.close_connection()

    async def close_connection(self):
        self.writer.close()
        await self.writer.wait_closed()


class VASP:
    client = None
    filepath = None
    isDone = False
    output_path = ""

    def __init__(self):
        # 初始化 RemoteClient 实例作为 VASP 类的成员变量
        self.client = RemoteClient(vasp_host_ip, vasp_host_port)
        self.filepath = None
        self.isDone = False
        # 生成一个由当前时间戳和随机数组成的目录名
        self.output_path = f"output_{int(time.time())}"
        os.makedirs(self.output_path, exist_ok=True)
        self.client.output_path = self.output_path
        print("Client initialized")

    async def connect_client(self):
        # 异步连接到远程服务器
        await self.client.connect()

    async def send_file(self, file_path):
        await self.client.send_file(file_path)

    async def send_command(self, command):
        await self.client.send_command(command)

    def load(self, file_path):
        self.filepath = file_path

    async def run(self):
        await self.send_file(self.filepath)
        self.isDone = True

    def getresult(self):
        pass

    def visualize(self):
        command = f'D:\VESTA\VESTA-win64\VESTA.exe" {self.output_path}\\CONTCAR'
        asyncio.run(self.send_command(command))
        pass

    def isDone(self):
        return self.isDone


def sendMessageToFUIDSL(fuldsisConnect, message):
    if token != 0:
        token_byte = token.to_bytes(4, byteorder="little", signed=True)
        message_len = len(message)
        message_len_byte = message_len.to_bytes(4, byteorder="little", signed=True)
        message_byte = message.encode("utf-8")  # 使用UTF-8编码
        message_all = token_byte + message_len_byte + message_byte
        fuldsisConnect.sendall(message_all)


def parseFromFULDSIMessage(data):
    f_token = data[:4]
    mess_len = data[4:8]
    token_ = int.from_bytes(f_token, byteorder="little")
    message_len_ = int.from_bytes(mess_len, byteorder="little")
    message_bytes = data[8: (8 + message_len_)]
    message = message_bytes.decode()
    return message


async def run_vasp():
    global vasp
    vasp = VASP()
    print("start vasp")
    await vasp.connect_client()
    vasp.load(filePath)
    await vasp.run()


async def startVASP():
    global vasp
    global lumapi
    global logFile
    global filePath
    logFile = "vasp_log.txt"
    exeFile = "C:/Users/PS/Documents/VESTA-win64/VESTA.exe"
    command = [exeFile, filePath]
    subprocess.Popen(command, stdout=subprocess.PIPE)


def isSimulationDone():
    if vasp != None:
        return vasp.isDone()
    return False


def getLogFilePath():
    if vasp != None:
        return vasp.output_path + "/vasp_log.txt"
    return ""


from fractions import Fraction
import numpy as np


def extract_formula(poscar_file):
    with open(poscar_file, "r") as file:
        lines = file.readlines()

    elements = lines[5].split()
    counts = list(map(int, lines[6].split()))

    # 计算最小公倍数以便体现比例
    total = sum(counts)
    ratios = [Fraction(count, total) for count in counts]

    # 计算最简比例
    ratios = [r.numerator for r in ratios]
    gcd = np.gcd.reduce(ratios)

    simplified_ratios = [r // gcd for r in ratios]

    formula = "".join(
        f"{elem}" + (f"{count}" if count > 1 else "")
        for elem, count in zip(elements, simplified_ratios)
    )
    return formula


def uploadResultToLib():
    # 上传结果到材料库，即material_lib_file这个文件
    # 从vasp.output_path中读取结果文件，然后写入material_lib_file, result文件是一个以results_开头的文件
    result_files = [f for f in os.listdir(vasp.output_path) if f.startswith("results_")]
    # check result文件的唯一性
    if len(result_files) != 1:
        print("Error: No result file found or multiple result files found")
        return
    result_file = result_files[0]
    result_file_path = os.path.join(vasp.output_path, result_file)

    formula = extract_formula(vasp.filepath)

    with open(result_file_path, "r") as f:
        result_data = f.read()
        # result文件的格式:
        # CONDUCTIVITY = 6.753670e+20 S/m
        # MOBILITY = 1.266439e+22 cm^2/Vs
        # 材料库文件的xml格式:
        # <MaterialProperties>
        #    <Material>
        #        <CommonProperties Name="EPOXY_ARAMID" Type="DIELECTRIC" Conductivity="0" DielConstant="3.9" LossTangent="0"/>
        #    </Material>
        #    <Material>
        #        <CommonProperties Name="FR-4" Type="DIELECTRIC" Conductivity="0" DielConstant="4.5" LossTangent="0.035"/>
        #    </Material>
        # </MaterialProperties>
        material_lib = None
        if os.path.exists(material_lib_file):
            with open(material_lib_file, "r") as f:
                material_lib = f.read()
        else:
            material_lib = "<MaterialProperties>\n</MaterialProperties>"
        # 解析result_data
        result_lines = result_data.split("\n")

        # 提取电导率，介电常数, 损耗角正切等信息
        conductivity = ""
        diel_constant = ""
        loss_tangent = ""
        mobility = ""
        density = ""
        thermal_expansion = ""
        thermal_conductivity = ""
        specific_heat = ""
        bandgap = ""

        for line in result_lines:
            if line.startswith("CONDUCTIVITY"):  # 提取电导率，去掉单位
                conductivity = line.split("=")[1].split()[0]
            elif line.startswith("DIELECTRIC"):
                diel_constant = line.split("=")[1].split()[0]
            elif line.startswith("LOSS TANGENT"):
                loss_tangent = line.split("=")[1].split()[0]
            elif line.startswith("MOBILITY"):
                mobility = line.split("=")[1].split()[0]
            elif line.startswith("DENSITY"):
                density = line.split("=")[1].split()[0]
            elif line.startswith("THERMAL EXPANSION"):
                thermal_expansion = line.split("=")[1].split()[0]
            elif line.startswith("THERMAL CONDUCTIVITY"):
                thermal_conductivity = line.split("=")[1].split()[0]
            elif line.startswith("SPECIFIC HEAT"):
                specific_heat = line.split("=")[1].split()[0]
            elif line.startswith("BANDGAP"):
                bandgap = line.split("=")[1].split()[0]
            else:
                continue

        # 更新material_lib
        conductivity_str = f'Conductivity="{conductivity}"' if conductivity else ""
        diel_constant_str = f'DielConstant="{diel_constant}"' if diel_constant else ""
        loss_tangent_str = f'LossTangent="{loss_tangent}"' if loss_tangent else ""
        mobility_str = f'Mobility="{mobility}"' if mobility else ""
        density_str = f'Density="{density}"' if density else ""
        thermal_expansion_str = f'ThermalExpansion="{thermal_expansion}"' if thermal_expansion else ""
        thermal_conductivity_str = f'ThermalConductivity="{thermal_conductivity}"' if thermal_conductivity else ""
        specific_heat_str = f'SpecificHeat="{specific_heat}"' if specific_heat else ""
        bandgap_str = f'Bandgap="{bandgap}"' if bandgap else ""
        material_properties = f'    <Material>\n\t<CommonProperties Name="{formula}" Type="DIELECTRIC"' + \
                              f' {density_str} {bandgap_str} {conductivity_str} {diel_constant_str} {loss_tangent_str} {mobility_str}  {thermal_expansion_str} {thermal_conductivity_str} {specific_heat_str}/>\n    </Material>\n'
        material_lib = material_lib.replace(
            "</MaterialProperties>", material_properties
        )
        material_lib += "</MaterialProperties>"
        with open(material_lib_file, "w") as f:
            f.write(material_lib)
        print(f"Material properties updated in {material_lib_file}")

        return


def visualVASPInner():
    global vasp
    exeFile = "C:/Users/PS/Documents/VESTA-win64/VESTA.exe"
    command = [exeFile, vasp.output_path + "/CONTCAR"]
    subprocess.Popen(command, stdout=subprocess.PIPE)
    time.sleep(3)
    command1 = [exeFile, vasp.output_path + "/CHGCAR"]
    subprocess.Popen(command1, stdout=subprocess.PIPE)


def visualVASP():
    executor.submit(visualVASPInner)


def create_tcp_connection(ip, port):
    # 创建一个新的socket对象
    fuldsisConnect = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # 连接到指定的IP和端口
    fuldsisConnect.connect((ip, port))
    sendMessageToFUIDSL(fuldsisConnect, "Connection successful.")
    asyncio.run(startVASP())
    print("Connected to {}:{}".format(ip, port))
    return fuldsisConnect


def listen_for_data(fuldsisConnect):
    while True:
        # 接收数据（最多1024字节）
        data = fuldsisConnect.recv(1024)
        if not data:
            # 如果没有收到数据，说明连接已经关闭
            break
        # print("Received data:", data.decode())
        cmd = parseFromFULDSIMessage(data)
        if cmd == "AnalyzeAll":
            path = "Analyze_Log_File " + getLogFilePath()
            sendMessageToFUIDSL(fuldsisConnect, path)
            asyncio.run(run_vasp())
            uploadResultToLib()
            sendMessageToFUIDSL(fuldsisConnect, "Analyze Done.")
            visualVASPInner()
        if cmd == "ShowResultView":
            visualVASPInner()


if __name__ == "__main__":
    # 获取当前文件所在的目录
    cwd = os.path.dirname(os.path.abspath(__file__))
    filePath = sys.argv[1]
    ipandPort = sys.argv[2].split(":")
    ip = ipandPort[0]
    port = ipandPort[1]
    token = int(sys.argv[3])
    # # 请替换为你要连接的端口号
    fuldsisConnect = create_tcp_connection(ip, int(port))
    listen_for_data(fuldsisConnect)
    fuldsisConnect.close()