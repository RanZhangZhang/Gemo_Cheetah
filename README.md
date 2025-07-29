# Gemo_Cheetah
This repo contains a  implementation for [Cheetah paper](https://eprint.iacr.org/2022/207)+ [Geometric Perspective](https://eprint.iacr.org/2025/200).
More information of codes see [OpenCheetah](https://github.com/Alibaba-Gemini-Lab/OpenCheetah).

### Repo Directory Description (changed file)
- `networks/main_module.cpp` cpp programs that evaluate our gemo_trunc protocol.
- `scripts/run-xx-test.sh` Helper scripts used to build the programs, where xx=server/client.

### Building Dependencies
* Run `bash scripts/build-deps.sh` 
  
### Building Cheetah Demo
* Run `bash scripts/build.sh` which will build 6 executables in the `build/bin` folder

### Local Demo 
1. On one terminal run `bash scripts/run-server-test.sh cheetah module`.
2. On other terminal run `bash scripts/run-client-test.sh cheetah module`. 


### Mimic an WAN setting within LAN on Linux
* To use the throttle script under [scripts/throttle.sh](scripts/throttle.sh) to limit the network speed and ping latency (require `sudo`)
* For example, run `sudo scripts/throttle.sh wan` on a Linux OS which will limit the local-loop interface to about 400Mbps bandwidth and 40ms ping latency.
  You can check the ping latency by just `ping 127.0.0.1`. The bandwidth can be check using extra `iperf` command.
