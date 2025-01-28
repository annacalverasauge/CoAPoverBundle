# emisor.py
import sys
sys.path.append('/home/max/Documentos/TFG/ud3tn/python-ud3tn-utils')  # Ruta al directorio 'python-ud3tn-utils'

from ud3tn_utils.aap2.aap2_client import AAP2AsyncUnixClient 
from ud3tn_utils.aap2.aap2_client import *
from ud3tn_utils.aap.aap_message import AAPMessage, AAPMessageType 

import asyncio
from ud3tn_utils.aap2.generated import aap2_pb2


SERVER_ADDRESS = 'ud3tn-a.aap2.socket'  # Servidor AAP local
SERVER_PORT = 4242

async def send_message():
    try:
        client = AAP2AsyncUnixClient(SERVER_ADDRESS)  
        #client.node_eid = "dtn://a.dtn/"
        await client.connect()
        await client.configure(agent_id ='a')
        await client._welcome()
        

        #print(client.eid())
        # Crear el objeto BundleADU
        bundle_data = b'Hello from sender!' 
        #message = AAPMessage(msg_type=AAPMessageType.SENDBUNDLE,eid = 'dtn://b.dtn/bundlesink', payload = "Hola", )
        adu_msg = aap2_pb2.BundleADU(
            src_eid="dtn://a.dtn/a",
            dst_eid="dtn://b.dtn/bundlesink",
            payload_length=len(bundle_data)
        )
        print(adu_msg)
        # Enviar el send_adu
        await client.send_adu(adu_msg, bundle_data)  
        #await client.send(message)

        print("Mensaje enviado")
    except AAP2CommunicationError as e:
        print(e)
    except AAP2ServerDisconnected as e:
        print(e)
    except AAP2OperationFailed as e:
        print(e)
    except AAP2UnexpectedMessage as e:
        print(e)
    finally:
        await client.disconnect()

asyncio.run(send_message())