LMNode Build Instructions and Notes
=============================
 - Version 1.0.1
 - Date: 25 February 2017
 - More detailed guide available here: http://hppcoin.org/LMNSetupGuide.pdf

Prerequisites
-------------
 - Ubuntu 16.04+
 - Libraries to build from hppcoin source
 - Port **28878** is open

Step 1. Build
----------------------
**1.1.**  Check out from source:

    git clone https://github.com/hppcoin/hppcoin

**1.2.**  See [README.md](README.md) for instructions on building.

Step 2. (Optional - only if firewall is running). Open port 28878
----------------------
**2.1.**  Run:

    sudo ufw allow 28878
    sudo ufw default allow outgoing
    sudo ufw enable

Step 3. First run on your Local Wallet
----------------------
**3.0.**  Go to the checked out folder

    cd hppcoin

**3.1.**  Start daemon :

    ./src/hppcoind -daemon -server

**3.2.**  Generate lmnodeprivkey:

    ./src/hppcoin-cli lmnode genkey

(Store this key)

**3.3.**  Get wallet address:

    ./src/hppcoin-cli getaccountaddress 0

**3.4.**  Send to received address **exactly 1000 HPP** in **1 transaction**. Wait for 8 confirmations.

**3.5.**  Stop daemon:

    ./src/hppcoin-cli stop

Step 4. In your VPS where you are hosting your LMNode. Update config files
----------------------
**4.1.**  Create file **hppcoin.conf** (in folder **~/.hppcoin**)

    rpcuser=username
    rpcpassword=password
    rpcallowip=127.0.0.1
    debug=1
    txindex=1
    daemon=1
    server=1
    listen=1
    maxconnections=24
    lmnode=1
    lmnodeprivkey=XXXXXXXXXXXXXXXXX  ## Replace with your lmnode private key
    externalip=XXX.XXX.XXX.XXX:28878 ## Replace with your node external IP

**4.2.**  Create file **lmnode.conf** (in folder **~/.hppcoin**) contains the following info:
 - LABEL: A one word name you make up to call your node (ex. LMN1)
 - IP:PORT: Your lmnode VPS's IP, and the port is always 28878.
 - LMNODEPRIVKEY: This is the result of your "lmnode genkey" from earlier.
 - TRANSACTION HASH: The collateral tx. hash from the 1000 HPP deposit.
 - INDEX: The Index is always 0 or 1.

To get TRANSACTION HASH, run:

    ./src/hppcoin-cli lmnode outputs

The output will look like:

    { "7a6527d45a50c24121776281470cffeba0e88099f12e77c5987a4b8e990b3af0" : "0", }

Sample of lmnode.conf:

    LMN1 51.52.53.54:28878 7E9t9kq7QT7QMVGUx9drgBpWbSXNdEBZ3cfkiLybxg8cKqr 7a6527d45a50c24121776281470cffeba0e88099f12e77c5987a4b8e990b3af0 0

Step 5. Run a lmnode
----------------------
**5.1.**  Start lmnode:

    ./src/hppcoin-cli lmnode start-alias <LABEL>

For example:

    ./src/hppcoin-cli lmnode start-alias LMN1

**5.2.**  To check node status:

    ./src/hppcoin-cli lmnode debug

If not successfully started, just repeat start command