#!/bin/bash
#
#
#  +-+-+-+ +-+-+-+-+-+-+ +-+-+ +-+-+-+-+-+-+
#  |A|W|S| |R|A|P|I|D|S| |M|N| |S|C|R|I|P|T|
#  +-+-+-+ +-+-+-+-+-+-+ +-+-+ +-+-+-+-+-+-+
#
#  +-+-+-+-+-+-+-+ +-+-+-+ +-+-+-+-+-+ +-+-+-+-+
#  |W|r|i|t|t|e|n| |b|y|:| |M|o|r|n|e| |B|e|c|k|
#  +-+-+-+-+-+-+-+ +-+-+-+ +-+-+-+-+-+ +-+-+-+-+
#
#  +-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+
#  |0|8| |D|e|c|e|m|b|e|r| |2|0|1|8|
#  +-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+
#
#

TMP_FOLDER=$(mktemp -d)
CONFIG_FILE='rapids.conf'
CONFIGFOLDER='/root/.rapids'
COIN_DAEMON='rapidsd'
COIN_CLI='rapids-cli'
COIN_PATH='/usr/local/bin/'
KERN_ARCH=$(getconf LONG_BIT)
COIN_TGZ="https://github.com/RapidsOfficial/Rapids/releases/download/v1.0.0.1/rapids-linux-${KERN_ARCH}.tar.gz"
COIN_ZIP=$(echo $COIN_TGZ | awk -F'/' '{print $NF}')
COIN_NAME='Rapids'
COIN_SERVICE='rapids'
COIN_PORT=28732
RPC_PORT=56001

NODEIP=$(curl -s4 icanhazip.com)

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'


function checks() {
  if [[ $(lsb_release -d) != *16.04* ]] && [[ $(lsb_release -d) != *17.10* ]] && [[ $(lsb_release -d) != *18.04* ]]; then
    echo -e "${RED}You are not running Ubuntu 16.04 or 18.04. Installation is cancelled.${NC}"
    exit 1
  fi

  if [[ $EUID -ne 0 ]]; then
     echo -e "${RED}$0 must be run as root.${NC}"
     exit 1
  fi

  if [ -n "$(pidof $COIN_DAEMON)" ] || [ -e "$COIN_DAEMON" ]; then
    echo -e "${RED}$COIN_NAME is already installed.${NC}"
    exit 1
  fi
}

function prepare_system() {
  echo -e "Preparing the system to install ${GREEN}$COIN_NAME${NC} masternode."
  echo -e "This might take up to 15 minutes, please be patient."
  sleep 3
  apt-get update
  mkdir -p .rapids
  DEBIAN_FRONTEND=noninteractive apt-get update
  DEBIAN_FRONTEND=noninteractive apt-get -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" -y -qq upgrade
  apt install -y software-properties-common
  echo -e "${GREEN}Adding bitcoin PPA repository...${NC}"
  apt-add-repository -y ppa:bitcoin/bitcoin
  echo -e "${GREEN}Installing required packages, it may take some time to finish.${NC}"
  apt-get update
  apt-get install -y -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" \
    sudo ufw git wget curl make automake autoconf build-essential libtool pkg-config libssl-dev \
    libboost-dev libboost-chrono-dev libboost-filesystem-dev libboost-program-options-dev \
    libboost-system-dev libboost-test-dev libboost-thread-dev libdb4.8-dev bsdmainutils \
    libdb4.8++-dev libminiupnpc-dev libgmp3-dev libevent-dev libdb5.3++ unzip libzmq5 nano
  if [ "$?" -gt "0" ]; then
    echo -e "${RED}Not all required packages were installed properly. Try to install them manually by running the following commands:${NC}\n"
    echo "apt-get update"
    echo "apt -y install software-properties-common"
    echo "apt-add-repository -y ppa:bitcoin/bitcoin"
    echo "apt-get update"
    echo "apt-get install sudo ufw git wget curl make automake autoconf build-essential libtool pkg-config libssl-dev \
      libboost-dev libboost-chrono-dev libboost-filesystem-dev libboost-program-options-dev \
      libboost-system-dev libboost-test-dev libboost-thread-dev libdb4.8-dev bsdmainutils \
      libdb4.8++-dev libminiupnpc-dev libgmp3-dev libevent-dev libdb5.3++ unzip libzmq5"
    exit 1
  fi
}

function download_node() {
  echo -e "Fetching ${GREEN}$COIN_NAME${NC} binary distribution..."
  cd $CONFIGFOLDER
  wget $COIN_TGZ
  if [ "$?" -gt "0" ];
   then
    echo -e "${RED}Failed to download $COIN_NAME. Please investigate.${NC}"
    exit 1
  fi
  tar xvzf $COIN_ZIP
  cp $COIN_DAEMON $COIN_CLI $COIN_PATH
  cd - &>/dev/null
  rm -rf $TMP_FOLDER
}

function get_ip() {
  declare -a NODE_IPS
  for ips in $(netstat -i | awk '!/Kernel|Iface|lo/ {print $1," "}'); do
    NODE_IPS+=($(curl --interface $ips --connect-timeout 2 -s4 icanhazip.com))
  done

  if [ ${#NODE_IPS[@]} -gt 1 ]
    then
      echo -e "${GREEN}More than one IP. Please type 0 to use the first IP, 1 for the second and so on...${NC}"
      INDEX=0
      for ip in "${NODE_IPS[@]}"; do
        echo ${INDEX} $ip
        let INDEX=${INDEX}+1
      done
      read -e choose_ip
      NODEIP=${NODE_IPS[$choose_ip]}
  else
    NODEIP=${NODE_IPS[0]}
  fi
}

function create_config() {
  mkdir -p $CONFIGFOLDER
  RPCUSER=$(tr -cd '[:alnum:]' < /dev/urandom | fold -w10 | head -n1)
  RPCPASSWORD=$(tr -cd '[:alnum:]' < /dev/urandom | fold -w22 | head -n1)
  cat <<EOF >$CONFIGFOLDER/$CONFIG_FILE
rpcuser=$RPCUSER
rpcpassword=$RPCPASSWORD
#rpcport=$RPC_PORT
#rpcallowip=127.0.0.1

listen=1
server=1
daemon=0

#bind=$NODEIP
port=$COIN_PORT

EOF
}

function create_key() {
  echo -e "Enter your ${RED}$COIN_NAME Masternode Private Key${NC}. Leave it blank to generate a new ${RED}Masternode Private Key${NC} for you:"
  read -e COINKEY
  if [[ -z "$COINKEY" ]]; then
    $COIN_PATH$COIN_DAEMON -daemon
    sleep 30
    if [ -z "$(ps axo cmd:100 | grep $COIN_DAEMON)" ]; then
      echo -e "${RED}$COIN_NAME server couldn not start. Check /var/log/syslog for errors.{$NC}"
      exit 1
    fi
    COINKEY=$($COIN_PATH$COIN_CLI masternode genkey)
    if [ "$?" -gt "0" ]; then
      echo -e "${RED}Wallet not fully loaded. Let us wait and try again to generate the Private Key${NC}"
      sleep 30
      COINKEY=$($COIN_PATH$COIN_CLI masternode genkey)
    fi
    $COIN_PATH$COIN_CLI stop
  fi
}

function update_config() {
  cat <<EOF >>$CONFIGFOLDER/$CONFIG_FILE
masternode=1
masternodeprivkey=$COINKEY
externalip=$NODEIP:$COIN_PORT
EOF
}

function enable_firewall() {
  echo -e "Installing and setting up firewall to allow ingress on port ${GREEN}$COIN_PORT${NC}"
  ufw allow $COIN_PORT/tcp comment "$COIN_NAME P2P port"
  ufw allow ssh comment "SSH"
  ufw limit ssh/tcp
  ufw default allow outgoing
  echo "y" | ufw enable
}

function configure_systemd() {
  cat <<EOF >/etc/systemd/system/$COIN_SERVICE.service
[Unit]
Description=$COIN_NAME service
After=network.target

[Service]
User=root
Group=root

Type=forking
#PIDFile=$CONFIGFOLDER/$COIN_NAME.pid

ExecStart=$COIN_PATH$COIN_DAEMON -datadir=$CONFIGFOLDER -conf=$CONFIGFOLDER/$CONFIG_FILE -daemon
ExecStop=-$COIN_PATH$COIN_CLI -datadir=$CONFIGFOLDER -conf=$CONFIGFOLDER/$CONFIG_FILE stop

Restart=always
PrivateTmp=true
TimeoutStopSec=60s
TimeoutStartSec=10s
StartLimitInterval=120s
StartLimitBurst=5

[Install]
WantedBy=multi-user.target
EOF

  systemctl daemon-reload
  sleep 3
  systemctl start $COIN_SERVICE.service
  systemctl enable $COIN_SERVICE.service

  if [[ -z "$(ps axo cmd:100 | egrep $COIN_DAEMON)" ]]; then
    echo -e "${RED}$COIN_NAME is not running${NC}, please investigate. You should start by running the following commands as root:"
    echo -e "${GREEN}systemctl start $COIN_SERVICE.service"
    echo -e "systemctl status $COIN_SERVICE.service"
    echo -e "less /var/log/syslog${NC}"
    exit 1
  fi
}

function important_information() {
  echo
  echo
  echo -e "${GREEN}================================================================================================================================${NC}"
  echo -e "$COIN_NAME masternode is up and running listening on ${RED}$NODEIP:$COIN_PORT${NC}."
  echo -e "Configuration file is: ${RED}$CONFIGFOLDER/$CONFIG_FILE${NC}"
  echo -e "Start:  ${RED}systemctl start  $COIN_SERVICE.service${NC}"
  echo -e "Stop:   ${RED}systemctl stop   $COIN_SERVICE.service${NC}"
  echo -e "Status: ${RED}systemctl status $COIN_SERVICE.service${NC}"
  echo -e "Masternode status: ${RED}$COIN_CLI masternode status${NC}"
  echo -e "MASTERNODE PRIVATEKEY is: ${RED}$COINKEY${NC}"
  if [[ -n $SENTINEL_REPO ]]; then
    echo -e "${RED}Sentinel${NC} is installed in ${RED}$CONFIGFOLDER/sentinel${NC}"
    echo -e "Sentinel logs is: ${RED}$CONFIGFOLDER/sentinel.log${NC}"
  fi
  echo -e "${GREEN}================================================================================================================================${NC}"
  echo -e "Create a new receiving address using your (desktop) cold wallet."  
  echo -e "Send exactly 1000000 (10Million) coins to the new receiving address using your (desktop) cold wallet."
  echo -e "After at least 1 confirmation then enter the following command in your wallet debug console: ${RED}masternode outputs${NC}"
  echo -e "You should have a masternode collateral transaction hash and index (usually 0 or 1)."
  echo -e "Edit ${RED}masternode.conf${NC} file in your (desktop) cold wallet data directory and copy and add the following line:"
  echo -e "${GREEN}mn1 $NODEIP:$COIN_PORT $COINKEY your-tx-hash your-tx-index${NC}"
  echo -e "Restart your (desktop) wallet, wait for at least 15 confirmations of collateral tx and start your masternode."
  echo -e "${GREEN}================================================================================================================================${NC}"
  echo -e "This script is provided by, ${RED}https://github.com/awsafrica/${NC}"
  echo -e "Used according to GNU GPL 3.0 terms and conditions."
  echo
  echo -e "${GREEN}================================================================================================================================${NC}"
}

##### Entry point #####
checks
prepare_system
download_node
get_ip
create_config
create_key
update_config
enable_firewall
configure_systemd
important_information
