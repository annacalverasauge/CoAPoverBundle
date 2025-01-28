#NodeB
import sys
import json
sys.path.append('/home/max/Documentos/TFG/ud3tn/python-ud3tn-utils')  # Ruta al directorio 'python-ud3tn-utils'
import struct

from ud3tn_utils.aap2.aap2_client import AAP2AsyncUnixClient 
from ud3tn_utils.aap2.aap2_client import *

import asyncio
from ud3tn_utils.aap2.generated import aap2_pb2
from aiocoap.message import Message
from aiocoap.numbers.codes import Code
from aiocoap.numbers.types import Type
from aiocoap import *
from aiocoap.message import UndecidedRemote
from aiocoap.transports.udp6 import UDP6EndpointAddress


SERVER_ADDRESS = 'ud3tn-b.aap2.socket'  # Socket AAP2 local
send_client = AAP2AsyncUnixClient(SERVER_ADDRESS)
receive_client = AAP2AsyncUnixClient(SERVER_ADDRESS)

async def main(send_client, receive_client):

    async with send_client, receive_client:
        secret = await send_client.configure(agent_id ='snd')
        await receive_client.configure(agent_id ='rec', subscribe=True)
        
        await asyncio.gather(
            #chat_send(send_client),
            chat_recive(receive_client)
        )


async def chat_send(send_client):
    try:
        while True:
            pay = input("Message or exit to escape: ")
            if pay.lower() == "exit":
                break
            payload = pay.encode("utf-8")
            print(payload)
            await send_client.send_adu(
            aap2_pb2.BundleADU(
                dst_eid="dtn://a.dtn/rec",
                payload_length=len(payload),
            ),
            payload,
            )
            response = await send_client.receive_response()
            assert response.response_status == aap2_pb2.ResponseStatus.RESPONSE_STATUS_SUCCESS
            print(response)
            
    finally:
        await send_client.disconnect()
  

async def chat_recive(receive_client):
    try:
        while True:

            # Receive the ADU.
            adu_msg, recv_payload = await receive_client.receive_adu()
            # Print and check what we received.
            print(adu_msg)
            print(recv_payload)

            print(f"Received ADU from {adu_msg.src_eid}: {recv_payload}")
            print("GET token and message ID")
            (vttkl, _, mid) = struct.unpack("!BBH", recv_payload[:4])
            token_length = vttkl & 0x0F
            token = recv_payload[4 : 4 + token_length]

            print(token)

            protocol = await Context.create_client_context()
           
            request_aio = Message.decode(recv_payload)
            print(request_aio)
            host = request_aio.opt.uri_host
            port = request_aio.opt.uri_port or 5683
            # Resolver 'localhost' o cualquier nombre de host.
            try:
                if host == "localhost":
                    host = "::1"  # IPv6 para localhost.
                else:
                    host = socket.gethostbyname(host)  # Resolver nombres de host.
            except socket.gaierror as e:
                print(f"Error resolving host '{host}': {e}")
                continue

            request_aio.remote = UDP6EndpointAddress(
                sockaddr=(host,port,0,0),
                interface= protocol
            )
            #request_aio.remote = UndecidedRemote(scheme ='coap',hostinfo = request_aio.opt.uri_host)
            #request_aio.mid = mid
            
            print(request_aio)
            print(request_aio.token)
            response = await protocol.request(request_aio).response
            print(request_aio)
            print(response)
            print(response.token)
            
            # Tell µD3TN that receiving the ADU went well.
            await receive_client.send_response_status(
                aap2_pb2.ResponseStatus.RESPONSE_STATUS_SUCCESS
            )
            #Ara s'ha d'enviar la resposta del aiocoap, decidir si ho faig en resposta o enviant un altre bundle
            #S'ha d'enviar una resposta de forma que el AAP2 del NodeA rebi un status code i un missathge aiocoap
            payload = response.encode()
            await send_client.send_adu(
            aap2_pb2.BundleADU(
                dst_eid="dtn://a.dtn/rec",
                payload_length=len(payload)
            ),
            payload,
            )
            response = await send_client.receive_response()
            #print("response of the sendclient")
            #print(response)

    finally:
        await receive_client.disconnect()

asyncio.run(main(send_client,receive_client))