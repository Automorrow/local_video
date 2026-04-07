#!/bin/bash
# WSL 端口转发自动更新脚本
# 添加到 ~/.bashrc: source /path/to/.wsl-port-forward.sh

WSL_IP=$(hostname -I | awk '{print $1}')
WIN_IP=$(ip route | grep default | awk '{print $3}')

echo "[WSL Port Forward] Current WSL IP: $WSL_IP"

# 删除旧规则并添加新规则
powershell.exe -Command "netsh interface portproxy delete v4tov4 listenport=8080 listenaddress=0.0.0.0" 2>/dev/null
powershell.exe -Command "netsh interface portproxy add v4tov4 listenport=8080 listenaddress=0.0.0.0 connectport=8080 connectaddress=$WSL_IP" 2>/dev/null

echo "[WSL Port Forward] Updated port 8080 -> $WSL_IP:8080"
echo "[WSL Port Forward] Access via: http://$WIN_IP:8080/"
