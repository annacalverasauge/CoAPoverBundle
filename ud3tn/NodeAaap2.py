#NodeA
import sys
import json
import aioconsole # type: ignore
import random

sys.path.append('/home/max/Documentos/TFG/ud3tn/python-ud3tn-utils')  # Ruta al directorio 'python-ud3tn-utils'

from ud3tn_utils.aap2.aap2_client import AAP2AsyncUnixClient 
from ud3tn_utils.aap2.aap2_client import *

import asyncio
from ud3tn_utils.aap2.generated import aap2_pb2
from aiocoap.message import Message
from aiocoap.numbers.codes import Code
from aiocoap.numbers.types import Type
from aiocoap.oscore import SecurityContextUtils
from aiocoap import *
from aiocoap.tokenmanager import *

SERVER_ADDRESS = 'ud3tn-a.aap2.socket'  # Socket AAP2 local
send_client = AAP2AsyncUnixClient(SERVER_ADDRESS)
receive_client = AAP2AsyncUnixClient(SERVER_ADDRESS)
MAX_ID = 65535
MIN_ID = 1
current_id = 1

async def main(send_client, receive_client):

    async with send_client, receive_client:
        secret = await send_client.configure(agent_id ='snd')
        await receive_client.configure(agent_id ='rec', subscribe=True)
        await asyncio.gather(
            chat_send(send_client),
            chat_receive(receive_client)
        )


async def chat_send(send_client):
    try:
        while True:
            
            message = await aioconsole.ainput("Message or exit to escape: ")
            #crear un context i fer missatges a partir de context.
            protocol = await Context.create_client_context()
            tman = TokenManager(protocol)

            if message.lower() == "exit":
                break
            elif message.lower() == "get":
                print("get 1")
                payload = Message(code=Code.GET, uri="coap://localhost/temperature", mtype=Type.NON, mid=current_id)
            elif message.lower() == "get all":
                payload = Message(code=Code.GET, uri="coap://localhost/temperature?all", mtype=Type.NON, mid=current_id)
                payload.opt.uri_query = ["all"]

            else:
                temperatura = str(round(random.uniform(10,30),2))
                payload = Message(code=Code.PUT, uri="coap://localhost/temperature", mtype=Type.NON, mid=current_id, payload=temperatura.encode("utf-8"))
            
            payload.token = tman.next_token()
            next_mid()

            print("CoAP Message")
            print(payload)
            p = payload.encode()

            print("\nCoAP encoded Message")
            print(p)

            await send_client.send_adu(
            aap2_pb2.BundleADU(
                dst_eid="dtn://b.dtn/rec",
                payload_length=len(p),
            ),
            p,
            )
            print("\nBundle created")
            print(aap2_pb2.BundleADU( dst_eid="dtn://b.dtn/rec",payload_length=len(p)))
            print(p)

            response = await send_client.receive_response()
            assert response.response_status == aap2_pb2.ResponseStatus.RESPONSE_STATUS_SUCCESS
            print("\nLa responsta")
            print(response)
            
    finally:
        await send_client.disconnect()
  

async def chat_receive(receive_client):
    try:
        while True:

            # Receive the ADU.
            adu_msg, recv_payload = await receive_client.receive_adu()
            print(f"Received ADU from {adu_msg.src_eid}: {recv_payload}")
            print(Message.decode(recv_payload).payload)
            
            # Tell µD3TN that receiving the ADU went well.
            await receive_client.send_response_status(
                aap2_pb2.ResponseStatus.RESPONSE_STATUS_SUCCESS
            )

    finally:
        await receive_client.disconnect()

def next_mid():
    global current_id
    current_id += 1
    if current_id > MAX_ID: 
        current_id = MIN_ID


    


asyncio.run(main(send_client,receive_client))