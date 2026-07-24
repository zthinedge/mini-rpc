#!/usr/bin/env bash

set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${MINIRPC_BUILD_DIR:-${root_dir}/build-zk}"
compose_file="${root_dir}/deployments/zookeeper/compose.yaml"
temp_dir="$(mktemp -d)"

server_9001_pid=""
server_9002_pid=""
server_9003_pid=""
client_pid=""

cleanup() {
    set +e

    for pid in \
        "${client_pid}" \
        "${server_9001_pid}" \
        "${server_9002_pid}" \
        "${server_9003_pid}"; do
        if [[ -n "${pid}" ]]; then
            kill "${pid}" 2>/dev/null
            wait "${pid}" 2>/dev/null
        fi
    done

    docker compose -f "${compose_file}" down -v >/dev/null 2>&1
    find "${temp_dir}" -type f -delete
    rmdir "${temp_dir}" 2>/dev/null
}
trap cleanup EXIT

if ! docker info >/dev/null 2>&1; then
    echo "Docker daemon is unavailable for the current user" >&2
    exit 1
fi

if [[ "${MINIRPC_SKIP_BUILD:-0}" != "1" ]]; then
    cmake \
        -S "${root_dir}" \
        -B "${build_dir}" \
        -DMINIRPC_WITH_ZOOKEEPER=ON
    cmake --build "${build_dir}" \
        --target registry_server registry_client \
        -j
fi

docker compose -f "${compose_file}" up -d --wait

"${build_dir}/registry_server" 9001 \
    >"${temp_dir}/server-9001.log" 2>&1 &
server_9001_pid=$!

"${build_dir}/registry_server" 9002 \
    >"${temp_dir}/server-9002.log" 2>&1 &
server_9002_pid=$!

sleep 2

"${build_dir}/registry_client" 127.0.0.1:2181 80 250 \
    >"${temp_dir}/client.log" 2>&1 &
client_pid=$!

sleep 2
before_9003_line="$(wc -l <"${temp_dir}/client.log")"

"${build_dir}/registry_server" 9003 \
    >"${temp_dir}/server-9003.log" 2>&1 &
server_9003_pid=$!

sleep 3

kill -KILL "${server_9001_pid}"
wait "${server_9001_pid}" 2>/dev/null || true
server_9001_pid=""

# ZooKeeper需要经过Session超时才能删除被SIGKILL进程的临时节点。
sleep 8
converged_line="$(wc -l <"${temp_dir}/client.log")"

wait "${client_pid}"
client_pid=""

initial_output="$(
    sed -n "1,${before_9003_line}p" "${temp_dir}/client.log"
)"
if ! rg -q 'selected=127\.0\.0\.1:9001' <<<"${initial_output}"||
   ! rg -q 'selected=127\.0\.0\.1:9002' <<<"${initial_output}"; then
    echo "initial two-instance round robin failed" >&2
    sed -n '1,120p' "${temp_dir}/client.log" >&2
    exit 1
fi

if ! rg -q 'selected=127\.0\.0\.1:9003' \
    "${temp_dir}/client.log"; then
    echo "client did not discover the new 9003 provider" >&2
    exit 1
fi

final_output="$(
    tail -n "+$((converged_line+1))" "${temp_dir}/client.log"
)"
if rg -q 'selected=127\.0\.0\.1:9001' <<<"${final_output}"; then
    echo "9001 was still selected after convergence" >&2
    exit 1
fi
if ! rg -q 'selected=127\.0\.0\.1:9002' <<<"${final_output}"||
   ! rg -q 'selected=127\.0\.0\.1:9003' <<<"${final_output}"; then
    echo "remaining providers were not both selected" >&2
    exit 1
fi

echo "ZooKeeper round-robin acceptance passed"
echo
sed -n '1,160p' "${temp_dir}/client.log"
