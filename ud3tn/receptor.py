# receptor.py
import sys
sys.path.append('/home/max/Documentos/TFG/ud3tn/python-ud3tn-utils')  # Ruta al directorio 'python-ud3tn-utils'

from ud3tn_utils.aap2.aap2_client import AAP2AsyncUnixClient  # Importación ahora debe funcionar
import asyncio

# Configuración del servidor AAP en el nodo receptor
SERVER_ADDRESS = 'ud3tn-b.aap2.socket'  # El servidor AAP local
SERVER_PORT = 4243

async def receive_message():
    try:
        client = AAP2AsyncUnixClient(SERVER_ADDRESS)  # Cambia 'localhost' y puerto según sea necesario
        client.node_eid = "dtn://b.dtn/"
        await client.connect()
        #await client._welcome()
        #print(client.eid())
        await client.configure(agent_id='bundlesink', secret = 'hola', subscribe = True )

        print("Esperando mensaje...")
        message = await client.receive_adu()
        print(f"Mensaje recibido: {message}") 
    except Exception as e:
        print(e)
    finally:
        await client.disconnect()

asyncio.run(receive_message())
