
// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2016-2017 The Zcoin developers
// Copyright (c) 2017-2018 The Hppcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SRC_LMNODECONFIG_H_
#define SRC_LMNODECONFIG_H_

class CLMNodeConfig;
extern CLMNodeConfig lmnodeConfig;

class CLMNodeConfig
{

public:

    class CLMNodeEntry {

    private:
        std::string alias;
        std::string ip;
        std::string privKey;
        std::string txHash;
        std::string outputIndex;
    public:

        CLMNodeEntry(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputIndex) {
            this->alias = alias;
            this->ip = ip;
            this->privKey = privKey;
            this->txHash = txHash;
            this->outputIndex = outputIndex;
        }

        const std::string& getAlias() const {
            return alias;
        }

        void setAlias(const std::string& alias) {
            this->alias = alias;
        }

        const std::string& getOutputIndex() const {
            return outputIndex;
        }

        void setOutputIndex(const std::string& outputIndex) {
            this->outputIndex = outputIndex;
        }

        const std::string& getPrivKey() const {
            return privKey;
        }

        void setPrivKey(const std::string& privKey) {
            this->privKey = privKey;
        }

        const std::string& getTxHash() const {
            return txHash;
        }

        void setTxHash(const std::string& txHash) {
            this->txHash = txHash;
        }

        const std::string& getIp() const {
            return ip;
        }

        void setIp(const std::string& ip) {
            this->ip = ip;
        }
    };

    CLMNodeConfig() {
        entries = std::vector<CLMNodeEntry>();
    }

    void clear();
    bool read(std::string& strErr);
    void add(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputIndex);

    std::vector<CLMNodeEntry>& getEntries() {
        return entries;
    }

    int getCount() {
        return (int)entries.size();
    }

private:
    std::vector<CLMNodeEntry> entries;


};


#endif /* SRC_LMNODECONFIG_H_ */
