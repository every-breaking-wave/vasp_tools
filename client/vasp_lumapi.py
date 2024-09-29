# def startSimulation()
# def stopSimulation()
# def hideResultView()
# def showResultView()
# def saveProject()
# def hideDock()
# def showDock()
# def loadProjectFile(fileName)
# def redirectResultPath(path)
# def receiveInputFile(fileArray)
# def requireMessage(messageType)

import asyncio
import os
import time
import struct

# TODO: 更改为实际的VASP服务器IP和端口
vasp_host_ip = '10.0.16.21'
vasp_host_port = 12345

class RemoteClient:

    def __init__(self, server, port):
        self.server = server
        self.port = port

    async def connect(self):
        self.reader, self.writer = await asyncio.open_connection(self.server, self.port)

    async def receive_file(self):
        print('Receiving file...')
        # 接收文件名长度（4 字节）
        # 先读第一条 COMPUTE finished \n
        # compute_finished = await self.reader.readuntil(b'\n')
        # print(f'Compute finished: {compute_finished}')
        filename_len_data = await self.reader.readexactly(4)
        print(f'Filename length data: {filename_len_data}')
        if not filename_len_data:
            return False  # 没有接收到数据，结束接收
        filename_len = struct.unpack('I', filename_len_data)[0]

        print(f'Filename length: {filename_len}')

        # 接收文件名
        filename = (await self.reader.readexactly(filename_len)).decode()

        if filename.startswith('END_OF_FILE'):
            return False
        
        # 接收文件大小（8 字节）
        filesize_data = await self.reader.readexactly(8)
        filesize = struct.unpack('Q', filesize_data)[0]

        # 接收文件内容
        with open(filename, 'wb') as f:
            bytes_received = 0
            while bytes_received < filesize:
                chunk_size = min(1024, filesize - bytes_received)
                chunk = await self.reader.readexactly(chunk_size)
                if not chunk:
                    break
                f.write(chunk)
                bytes_received += len(chunk)
            print(f"Received file: {filename} ({filesize} bytes)")
        

        return True


    async def send_file(self, file_path):
        file_name = os.path.basename(file_path)
        file_header = f"FILE{file_name}\n"
        
        try:
            with open(file_path, 'rb') as file:
                # Send file header
                self.writer.write(file_header.encode())
                await self.writer.drain()

                # Send file content
                chunk = file.read(1024)
                while chunk:
                    self.writer.write(chunk)
                    await self.writer.drain()
                    chunk = file.read(1024)

            print(f'File {file_name} sent successfully')

            # 发送文件结束标志
            end_signal = "END_OF_FILE\n"
            self.writer.write(end_signal.encode())
            await self.writer.drain()
            print('End of file signal sent')

            # Wait for the response
            print('Waiting for server response...')
            while True:
                    if not await self.receive_file():
                        break

        except Exception as e:
            print(f'Error sending file: {e}')
        finally:
            await self.close_connection()

    async def send_command(self, command):
        command_message = f"CMD {command}"
        try:
            self.writer.write(command_message.encode())
            await self.writer.drain()

            # Wait for the response
            response = await self.reader.read(1024)
            print(f'Server response: {response.decode()}')
        except Exception as e:
            print(f'Error sending command: {e}')
        finally:
            await self.close_connection()

    async def close_connection(self):
        self.writer.close()
        await self.writer.wait_closed()


class VASP:

    client = None
    filepath = None
    isDone = False
    output_path = None

    def __init__(self):
        # 初始化 RemoteClient 实例作为 VASP 类的成员变量
        self.client = RemoteClient(vasp_host_ip, vasp_host_port)
        self.filepath = None
        print('Client initialized')
    
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
        # 为每次运行创建一个新的输出目录
        self.output_path = f'output_{time.time()}'
        os.makedirs(self.output_path, exist_ok=True)
        
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