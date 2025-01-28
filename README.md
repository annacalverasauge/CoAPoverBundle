# CoAPoverBundle
CoAP over Bundle implementation using the libraries aiocoap and µD3TN. This repository adds and interaction layer between the Bundle Layer and the CoAP layer. To execute this code there are different steps needed.

## Part 1: Instalation and compilation of the requiered libraries.

```bash
sudo apt update
sudo apt install python3 pyhton3-pip
```
Then when installen the python language and the repositori, the ud3tn library must be compiled.
```bash
make posix
make run-posix
```
## Part 2: Deploymen of the nodes.

In this part it is explained how there are delpoyed two nodes, and the comunication between them, if more information is precised it can be founded in the ud3tn Repositori.

Node A:
```bash
cd /ud3tn
build/posix/ud3tn --node-id dtn://a.dtn/     --aap-port 4242 --aap2-socket ud3tn-a.aap2.socket     --cla "tcpclv3:*,4556"
```
Node B:
```bash
cd /ud3tn
build/posix/ud3tn --node-id dtn://b.dtn/     --aap-port 4243 --aap2-socket ud3tn-b.aap2.socket     --cla "tcpclv3:*,4225"
```

Communication between nodes:
```bash
cd /ud3tn
source .venv/bin/activate
aap2-config --socket ud3tn-a.aap2.socket     --schedule 1 3600 100000     dtn://b.dtn/ tcpclv3:localhost:4225
aap2-config --socket ud3tn-b.aap2.socket     --schedule 1 3600 100000     dtn://a.dtn/ tcpclv3:localhost:4556
```
This will deploy Node A and Node B with it's respective AAP2.0 sockets, that the integration developed will need, and a convergance layer tcpclv3 with and access port for each CL. It also establishes the communication between the nodes.


## Part 3: Execution of the code.

Fist of all the CoAP server needs to start running.
```bash
cd /aiocoap
python3 serverSensor.py
```

Then the app that will act as the encapsulation and decapsulation layer needs to run in different terminals.

 For Node B:
 ```bash
cd /ud3tn
source .venv/bin/activate
python3 NodeBaap2.py
```

For Node A:
 ```bash
cd /ud3tn
source .venv/bin/activate
python3 NodeAaap2.py
```

And scheme of the deployed code can be seen in the image below.
<img width="437" alt="image" src="https://github.com/user-attachments/assets/047ed7b4-743b-4ece-b32f-caff7e1dba4b" />










