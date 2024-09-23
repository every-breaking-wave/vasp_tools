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
import asyncio
import os
import time

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

# async def main():
#     client = RemoteClient('10.0.16.21', 12345)  # Replace with your server and port
#     await client.connect()

#     # Send a file to the server
#     await client.send_file('./fdtd.py')

#     # # Send a command to the server
#     # await client.connect()
#     # await client.send_command('ls -l')

# # Run the main function
# asyncio.run(main())




# 定义一个VASP类
class VASP:
    def __init__(self):
        pass

    def load(self, filePath):
        pass

    def run(self):
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
