import asyncio

import aiocoap.resource as resource
from aiocoap.numbers.contentformat import ContentFormat
import aiocoap



class TemperatureResource(resource.Resource):
    
    def __init__(self):
        super().__init__()
        self.temperatures = []  

    async def render_get(self, request):
        
        if "all" in request.opt.uri_query:
            print("GET all")
            payload = ", ".join(map(str, self.temperatures)).encode("utf-8")
        else:
            print("GET 1")
            payload = (
                str(self.temperatures[-1]).encode("utf-8")
                if self.temperatures
                else b"No temperatures recorded."
            )
        return aiocoap.Message(payload=payload)

    async def render_put(self, request):
        
        try:
            
            temperature = float(request.payload.decode("utf-8"))
            self.temperatures.append(temperature)
            print(f"Received temperature: {temperature}")
            return aiocoap.Message(code=aiocoap.CHANGED, payload=b"Temperature recorded.")
        except ValueError:
            
            return aiocoap.Message(code=aiocoap.BAD_REQUEST, payload=b"Invalid temperature.")


class ProbaPOST(resource.Resource):
    def __init__(self):
        super().__init__()
    async def render_post(self, request):
        print(request)
        return aiocoap.Message(code=aiocoap.CHANGED, payload=b"POST.")

class DynamicResourceCreator(resource.Resource):

    def __init__(self, root_site):
        super().__init__()
        self.root_site = root_site

    async def render_post(self, request):
       
        resource_id = request.opt.uri_path[-1] 
        if resource_id in self.root_site._resources:
            return aiocoap.Message(payload=b"El recurso ya existe.", code=aiocoap.FORBIDDEN)
        payload = request.payload.decode('utf-8')
        new_resource = DynamicResource(initial_data=payload)

        self.root_site.add_resource((resource_id,), new_resource)
        return aiocoap.Message(payload=f"Recurso '{resource_id}' creado.".encode('utf-8'), code=aiocoap.CREATED)



class DynamicResource(resource.Resource):
    def __init__(self, initial_data=""):
        super().__init__()
        self.data = initial_data

    async def render_get(self, request):
    
        return aiocoap.Message(payload=self.data.encode('utf-8'), code=aiocoap.CONTENT)

    async def render_post(self, request):
        payload = request.payload.decode('utf-8')
        self.data += f"\n{payload}"
        return aiocoap.Message(payload = b"Dades afegides", code=aiocoap.CHANGED)


async def main():
    # Resource tree creation
    root = resource.Site()

    root.add_resource(["temperature"], TemperatureResource())
    #root.add_resource([], ProbaPOST())
    #root.add_resource(['create'], DynamicResourceCreator(root))

    await aiocoap.Context.create_server_context(root)

    # Run forever
    await asyncio.get_running_loop().create_future()


if __name__ == "__main__":
    asyncio.run(main())