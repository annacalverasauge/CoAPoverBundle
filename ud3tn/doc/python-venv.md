# Python 3 virtualenv Setup

Multiple functional tests as well as the integration test toolchain in the µD3TN
project are using Python scripts to speed up development compared to pure C
test implementations. Some of the scripts are part of the `python-ud3tn-utils` package, while others are located in the [tools/](tools/) directory.
The integration test toolchain can be found in the
[test/integration/](test/integration/) directory. Both leverage a Python 3
implementation of several DTN protocols and convergence layers, provided in the
[pyd3tn/](pyd3tn/) directory.

A [venv](https://docs.python.org/3/library/venv.html) is used to isolate the
µD3TN Python environment from the system. The Makefile target `virtualenv`
creates a default Python virtualenv including all required Python packages.

```bash
make virtualenv
```

The location of the virtualenv directory can be controlled with the `VENV`
parameter. The default directory is `.venv/`.

```sh
make VENV=path/to/venv virtualenv
```

After the virtualenv is created the `activate` script has to be sourced to add
the virtualenv Python interpreter to the `PATH` variable. Alternatively,
environment switcher like `direnv` (see below) or `virtualenvwrapper` can be
used.

```sh
source .venv/bin/activate
```

The `tools/` directory is added to the virtualenv site-packages allowing the
import of packages residing in `tools/` like the `pyd3tn` package, e.g.

```python
from pyd3tn.bundle7 import serialize_bundle7

serialize_bundle7("dtn://GS1/", "dtn://GS2/", b"Hello world!")
```

## Use an own virtual environment handler

You may also use other tools or a manual approach for managing the virtual
environment, e.g., if you want it to use another path.
In this case, create and `activate` a new virtual environment using the method
of your choice and then install the dependencies:

```sh
make update-virtualenv
```

## Optional: direnv support

[direnv](https://direnv.net/) is an environment switcher loading environtal
variables depending on the current directory. It can be used to automatically
create and load Python virtualenvs. This is an example `.envrc` file making use
of the Makefile `virtualenv` target.

```bash
layout_virtualenv() {
    local venv=$1
    if [ ! -d "${venv}" ]; then
        make virtualenv
    fi
    source ${venv}/bin/activate
}

layout virtualenv .venv
```
