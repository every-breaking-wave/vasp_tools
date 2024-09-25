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
import aiohttp
import os
import time

# TODO: 更改为实际的VASP服务器IP和端口
vasp_host_ip = '10.0.16.21'
vasp_host_port = 12345

class RemoteClient:

    def __init__(self, server, port):
        self.server = server
        self.port = port

    async def connect(self):
        self.reader, self.writer = await asyncio.open_connection(self.server, self.port)
    
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
                # 持续接收服务器的消息，直到不再有消息
                response = await self.reader.read(1024)
                if not response:
                    break
                print(f'Server response: {response.decode()}')


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
        await self.send_file(self.filepath)

    def getresult(self):
        pass

    def visualize(self):
        pass

    def startSimulation():
        pass


    def stopSimulation():
        pass

    def hideResultView():
        pass


    def showResultView():
        pass

    def saveProject():
        pass

    def hideDock():
        pass

    def showDock():
        pass

    def loadProjectFile(fileName):
        pass

    def redirectResultPath(path):
        pass

    def receiveInputFile(fileArray):
        pass

    def requireMessage(messageType):
        pass