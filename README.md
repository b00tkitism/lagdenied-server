# LagDenied: A High-Performance UDP Proxy Architecture

**LagDenied** is a rigorously engineered UDP proxy system developed in **C**, leveraging the **libuv** asynchronous I/O library to facilitate high-throughput and low-latency datagram relaying. It has been purpose-built for real-time interactive applications—such as multiplayer gaming and voice-over-IP (VoIP) systems—where timing constraints and predictable delivery pathways are essential. LagDenied is architected around a **per-session socket model**, an efficient **lock-free hash table**, and a **preallocated buffer pool** to eliminate dynamic memory allocation during runtime.

> **Note:** This system is primarily intended for integration with Windows clients employing [WinPkFilter](https://www.ntkernel.com/winpkfilter/) for kernel-mode UDP packet redirection. The companion client repository is available at: [github.com/b00tkitism/lagdenied-client-win](https://github.com/b00tkitism/lagdenied-client-win).

---

## Key Capabilities

* Asynchronous event-driven core via **libuv**
* **Session-specific sockets** (i.e., one socket per client-remote pair)
* Real-time **hash-based session management** using `uthash`
* Custom **zero-allocation buffer pool** for deterministic memory usage
* Automated **session lifecycle tracking and reclamation**
* Designed to exhibit **minimal processing overhead** in latency-sensitive scenarios
* Easily integrable into systems employing **kernel-mode packet filters**

---

## Application Domains

* Transparent UDP proxying for online multiplayer game clients
* Secure or identity-masked transit layers for NAT-traversed UDP sessions
* Real-time traffic steering or content-based routing
* Selective packet inspection and rewriting via client-side redirection

---

## Architectural Model

```
[ Windows Host (with WinPkFilter) ]
              |
      [ LagDenied UDP Proxy ]
              |
     [ Remote Game / Media Server ]
```

* The Windows-based client modifies outbound UDP packets' destination using WinPkFilter.
* LagDenied accepts redirected packets and dynamically allocates a session-specific upstream socket for each `(client IP:port, remote IP:port)` tuple.
* Upstream responses are routed via the corresponding socket, preserving the expected source address tuple.

---

## Compilation and Setup

### Build Prerequisites

* A C99-compliant compiler (e.g., **GCC** or **Clang**)
* [libuv](https://github.com/libuv/libuv), version 1.46 or newer
* CMake
* `uthash.h` (included in the source tree)

### Compilation Steps

```bash
git clone https://github.com/b00tkitism/lagdenied.git
cd lagdenied
mkdir -p ./build
cd build
cmake ..
make
```

For CMake users, a custom `CMakeLists.txt` can be introduced for integration into larger toolchains.

---

## Performance Characteristics

LagDenied has demonstrated the capacity to sustain line-rate datagram throughput on 1GbE and 10GbE interfaces when paired with tuned system parameters and appropriate NIC offloads.

* Scales to **100,000+ packets per second** per core with zero dynamic allocations
* Lock-free data structures allow for constant-time session lookups
* Tested using high-concurrency UDP traffic generators and production VoIP/game traffic

To benchmark performance:

```bash
iperf -u -c <proxy-ip> -p <proxy-port> -b 100M -t 10
```

---

## Related Software

* **lagdenied-client-win**: [GitHub Repository](https://github.com/b00tkitism/lagdenied-client-win)
  Implements the client-side logic using WinPkFilter to intercept and reroute UDP packets from selected processes toward LagDenied proxy.

---

## Licensing

This project is licensed under the MIT License. Please consult the `LICENSE` file for the full legal text.

---

## Contributions

Community contributions are encouraged. Kindly submit pull requests or issues for enhancements, performance tuning, or feature proposals. All submissions should adhere to idiomatic C practices and retain cross-platform build compatibility.

---

## Acknowledgments

* [libuv](https://github.com/libuv/libuv): The foundation of the proxy's event-driven architecture
* [uthash](https://troydhanson.github.io/uthash/): Lightweight, embedded hash table library for C
* [WinPkFilter](https://www.ntkernel.com/winpkfilter/): Kernel-mode packet interception for Windows, used by the official client implementation
