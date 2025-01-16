#NodeA
import sys
import asyncio
from aiocoap import *
sys.path.append('/home/max/Documentos/TFG/Exemple_sensor/ud3tn/python-ud3tn-utils')  # Ruta al directorio 'python-ud3tn-utils'

from ud3tn_utils.aap2.aap2_client import AAP2AsyncUnixClient 
from ud3tn_utils.aap2.aap2_client import *
from ud3tn_utils.aap2.generated import aap2_pb2


SERVER_ADDRESS = 'ud3tn-b.aap2.socket'  # Servidor AAP local
send_client = AAP2AsyncUnixClient(SERVER_ADDRESS)
recive_client = AAP2AsyncUnixClient(SERVER_ADDRESS)

async def main(send_client, recive_client):

    async with send_client, recive_client:
        secret = await send_client.configure(agent_id ='snd')
        await recive_client.configure(agent_id ='rec', subscribe=True)
        await chat(send_client,recive_client)

async def chat(send_client, recive_client):
    try:
        while True:
            pay = input("Message or exit to escape: ")
            if pay.lower() == "exit":
                break
            payload = pay.encode("utf-8")
            await send_client.send_adu(
            aap2_pb2.BundleADU(
                dst_eid="dtn://a.dtn/rec",
                payload_length=len(payload),
            ),
            payload,
            )
            response = await send_client.receive_response()
            assert response.response_status == aap2_pb2.ResponseStatus.RESPONSE_STATUS_SUCCESS
            # Receive the ADU.
            adu_msg, recv_payload = await recive_client.receive_adu()
            # Print and check what we received.
            print(f"Received ADU from {adu_msg.src_eid}: {recv_payload.decode("utf-8")}")
            assert adu_msg.src_eid == send_client.eid
            assert recv_payload == payload
            # Tell µD3TN that receiving the ADU went well.
            await recive_client.send_response_status(
                aap2_pb2.ResponseStatus.RESPONSE_STATUS_SUCCESS
            )
    
    finally:
        await send_client.disconnect()
        await recive_client.disconnect()


asyncio.run(main(send_client,recive_client))