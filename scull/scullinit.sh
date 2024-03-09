#!/bin/bash

# scullinit.sh -- the shell script to load, unload, reload scull drivers
#
# Copyright (C) 2024  Arka Mondal

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
