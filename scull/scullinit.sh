#!/bin/bash

declare -i no_devs

# control variables
declare -r DEVICE="scull"   # the name of the device
declare -r MODULE="scull"   # the name of the module
mode="664"                  # permission
no_devs=4
group=""

function clean_devnodes()
{
  for ((i = 0; i < no_devs; i++))
  do
    rm -f /dev/${DEVICE}$i
  done

  return
}

function load() {

  # TODO: add support for the insmod arguments
  insmod ./${MODULE}.ko || exit 1

  clean_devnodes

  major=$(awk -v device="${DEVICE}" '$2 == "device" {print $1}' /proc/devices)

  for ((i = 0; i < no_devs; i++))
  do
    mknod /dev/${DEVICE}$i c "${major}" $i
  done

  if grep -qE '^wheel:' /etc/group; then
    group="wheel"
  elif grep -qE '^staff:' /etc/group; then
    group="staff"
  else
    group="root"
  fi

  for ((i = 0; i < no_devs; i++))
  do
    chgrp ${group} /dev/"${DEVICE}"$i
    chmod ${mode} /dev/"${DEVICE}"$i
  done

  return
}

function unload()
{
  rmmod "${MODULE}" && clean_devnodes || exit 1

  return
}

args=${1:-"load"}
case "${args}" in
  load )
    load ;;
  unload )
    unload ;;
  reload )
    unload
    load
    ;;
  help )
    echo "Usage: $0 [load | unload | reload]"
    echo "Default is load"
    exit 1
    ;;
  * )
    echo "Invalid argument. try help"
esac
