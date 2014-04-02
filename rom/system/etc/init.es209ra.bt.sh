#!/system/bin/sh

BLUETOOTH_SLEEP_PATH=/proc/bluetooth/sleep/proto
LOG_TAG="qcom-bluetooth"
LOG_NAME="${0}:"
hciattach_pid=""

loge ()
{
  /system/bin/log -t $LOG_TAG -p e "$LOG_NAME $@"
}

logi ()
{
  /system/bin/log -t $LOG_TAG -p i "$LOG_NAME $@"
}

failed ()
{
  loge "$1: exit code $2"
  exit $2
}

start_hciattach ()
{
  echo 1 > $BLUETOOTH_SLEEP_PATH
  /system/bin/hciattach -n $QSOC_DEVICE $QSOC_TYPE $QSOC_BAUD &
  hciattach_pid=$!
  logi "start_hciattach: pid = $hciattach_pid"
  /system/bin/abtfilt -c -d -z -n &
  abtfilt_pid=$!
  logi "start_abtfilt: pid = $abtfilt_pid"
}

kill_hciattach ()
{
  logi "kill_abtfilt: pid = $abtfilt_pid"
  kill -TERM $abtfilt_pid
  logi "kill_hciattach: pid = $hciattach_pid"
  kill -TERM $hciattach_pid
  echo 0 > $BLUETOOTH_SLEEP_PATH
}

USAGE="hciattach [-n] [-p] [-b] [-t timeout] [-s initial_speed] <tty> <type | id> [speed] [flow|noflow] [bdaddr]"

while getopts "blnpt:s:" f
do
  case $f in
  b | l | n | p)  opt_flags="$opt_flags -$f" ;;
  t)      timeout=$OPTARG;;
  s)      initial_speed=$OPTARG;;
  \?)     echo $USAGE; exit 1;;
  esac
done
shift $(($OPTIND-1))

QSOC_DEVICE=${1:-"/dev/ttyHS1"}
QSOC_TYPE=${2:-"any"}
QSOC_BAUD=${3:-"3000000"}

/system/bin/hci_qcomm_init -d $QSOC_DEVICE -s $QSOC_BAUD

exit_code_hci_qcomm_init=$?

case $exit_code_hci_qcomm_init in
  0) logi "Bluetooth QSoC firmware download succeeded";;
  *) failed "Bluetooth QSoC firmware download failed" $exit_code_hci_qcomm_init;;
esac

trap "kill_hciattach" TERM INT
start_hciattach
wait $hciattach_pid
logi "Bluetooth stopped"
exit 0
