#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

set -o errexit

# This assumes you are running the command from within the "ud3tn" directory.
UD3TN_DIR="$(pwd)"
cd "$UD3TN_DIR"

exit_handler() {
    cd "$UD3TN_DIR"

    kill -TERM $UD3TN1_PID || true
    kill -TERM $UD3TN2_PID || true
    kill -TERM $UD3TN3_PID || true
    kill -TERM $UD3TN4_PID || true

    echo "Terminating ION (timeout 20s)..."
    (ionstop || true) &
    sleep 20

    echo
    echo ">>> ION LOGFILE"
    cat "ion.log" || true
    echo
    echo ">>> LOWER1 LOGFILE"
    cat "/tmp/lower1.log" || true
    echo
    echo ">>> LOWER2 LOGFILE"
    cat "/tmp/lower2.log" || true
    echo
    echo ">>> UPPER1 LOGFILE"
    cat "/tmp/upper1.log" || true
    echo
    echo ">>> UPPER2 LOGFILE"
    cat "/tmp/upper2.log" || true
    echo

    echo "Waiting for uD3TN to exit gracefully - if it doesn't, check for sanitizer warnings."
    wait $UD3TN1_PID
    wait $UD3TN2_PID
    wait $UD3TN3_PID
    wait $UD3TN4_PID
}

rm -f ion.log
rm -f /tmp/ion*log /tmp/ud3tn*.log

# Start first uD3TN instance (lower1)
"$UD3TN_DIR/build/posix/ud3tn" -a localhost -p 4242 -S "$UD3TN_DIR/ud3tn1.aap2.socket" -e dtn://lower1.dtn/ -c "sqlite:ud3tn1.sqlite;tcpclv3:127.0.0.1,4555;mtcp:127.0.0.1,4224" -L 4 --allow-remote-config > /tmp/lower1.log 2>&1 &
UD3TN1_PID=$!

# Start second uD3TN instance (upper1)
"$UD3TN_DIR/build/posix/ud3tn" -a localhost -p 4243 -S "$UD3TN_DIR/ud3tn2.aap2.socket" -e dtn://upper1.dtn/ -c "sqlite:ud3tn2.sqlite;bibe:," -L 4 --allow-remote-config > /tmp/upper1.log 2>&1 &
UD3TN2_PID=$!

# Start third uD3TN instance (lower2)
"$UD3TN_DIR/build/posix/ud3tn" -a localhost -p 4244 -S "$UD3TN_DIR/ud3tn3.aap2.socket" -e dtn://lower2.dtn/ -c "sqlite:ud3tn3.sqlite;tcpclv3:127.0.0.1,4554" -L 4 --allow-remote-config > /tmp/lower2.log 2>&1 &
UD3TN3_PID=$!

# Start fourth uD3TN instance (upper2)
"$UD3TN_DIR/build/posix/ud3tn" -a localhost -p 4245 -S "$UD3TN_DIR/ud3tn4.aap2.socket" -e dtn://upper2.dtn/ -c "sqlite:ud3tn4.sqlite;bibe:," -L 4 --allow-remote-config > /tmp/upper2.log 2>&1 &
UD3TN4_PID=$!

# Start ION instance
ulimit -n 512 # fix behavior on systems with a huge limit (e.g. if the container runtime does not change the kernel default), see: #121
ionstart -I test/ion_interoperability/bibe_forwarding_test/ionstart.rc

# UD3TN1_PID, UD3TN2_PID, UD3TN3_PID, and UD3TN4_PID must be defined for this
trap exit_handler EXIT

# Configure contacts
sleep 3.5
HOST="$(hostname)"
aap-config --tcp localhost 4243 --dest_eid dtn://upper1.dtn/ --schedule 1 3600 100000 dtn://upper2.dtn/ "bibe:localhost:4242#dtn://$HOST/"
sleep 1.5
aap-config --tcp localhost 4242 --schedule 1 3600 10000 --reaches "dtn://$HOST/" dtn://ion.dtn/ tcpclv3:127.0.0.1:4556
sleep 1.5
aap-config --tcp localhost 4244 --dest_eid dtn://lower2.dtn/ --schedule 1 3600 10000 dtn://ion.dtn/ tcpclv3:127.0.0.1:4556
sleep 1.5
aap-config --tcp localhost 4245 --dest_eid dtn://upper2.dtn/ --schedule 1 3600 100000 dtn://upper2.dtn/ bibe:localhost:4244
sleep 1.5
# Send a BIBE bundle to lower1
PAYLOAD="THISISTHEBUNDLEPAYLOAD"
python "$UD3TN_DIR/tools/cla/bibe_over_mtcp_test.py" -l localhost -p 4224 --payload "$PAYLOAD" -i "dtn://upper2.dtn/bundlesink" -o "dtn://lower1.dtn/" --compatibility &

timeout -v 10 stdbuf -oL aap-receive --tcp localhost 4245 -a bundlesink --count 1 --verify-pl "$PAYLOAD" --newline -vv
