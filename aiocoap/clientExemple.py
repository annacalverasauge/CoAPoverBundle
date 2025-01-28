import asyncio
from aiocoap import *
import random
from aiocoap.tokenmanager import TokenManager
import os

async def main():
    protocol = await Context.create_client_context()
    tman = TokenManager(protocol)

    while True:

        message = input("Message option (GET or ALL or PUT): ")
        
        if message == "GET":
            request = Message(code=GET, uri="coap://localhost/temperature", mtype=NON)
            
        elif message == "ALL":
            request = Message(code=GET, uri="coap://localhost/temperature?all", mtype=NON)
        elif message == "POST":
            request = Message(code=POST, uri="coap://localhost/temperature", mtype=NON, payload=b'30')
        else:
            temperature = str(round(random.uniform(10,30),2)).encode("utf-8")
            request = Message(code=PUT, payload=temperature, uri="coap://localhost/temperature", mtype=NON)
        t = tman.next_token()
        request.token = t
        print(t)
        print(request)

        try:
            response = await protocol.request(request).response
            print(request)
            print(response)
        except Exception as e:
            print("Failed to fetch resource:")
            print(e)
        else:
            print("Result: %s\n%r" % (response.code, response.payload))


if __name__ == "__main__":
    asyncio.run(main())