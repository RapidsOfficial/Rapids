#!/usr/bin/env python3
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Utilities for manipulating blocks and transactions."""

from struct import pack

from test_framework.address import wif_to_privkey
from test_framework.key import CECKey
from test_framework.mininode import *
from test_framework.script import CScript, OP_TRUE, OP_CHECKSIG


# Create a block (with regtest difficulty)
def create_block(hashprev, coinbase, nTime=None):
    block = CBlock()
    if nTime is None:
        import time
        block.nTime = int(time.time()+600)
    else:
        block.nTime = nTime
    block.hashPrevBlock = hashprev
    block.nBits = 0x1e0ffff0 # Will break after a difficulty adjustment...
    block.nAccumulatorCheckpoint = 0
    block.vtx.append(coinbase)
    block.hashMerkleRoot = block.calc_merkle_root()
    block.calc_sha256()
    return block

def serialize_script_num(value):
    r = bytearray(0)
    if value == 0:
        return r
    neg = value < 0
    absvalue = -value if neg else value
    while (absvalue):
        r.append(int(absvalue & 0xff))
        absvalue >>= 8
    if r[-1] & 0x80:
        r.append(0x80 if neg else 0)
    elif neg:
        r[-1] |= 0x80
    return r

def cbase_scriptsig(height):
    return ser_string(serialize_script_num(height))

def cbase_value(height):
    #return ((50 * COIN) >> int(height/150))
    return (250 * COIN)

# Create a coinbase transaction, assuming no miner fees.
# If pubkey is passed in, the coinbase output will be a P2PK output;
# otherwise an anyone-can-spend output.
def create_coinbase(height, pubkey = None):
    coinbase = CTransaction()
    coinbase.vin = [CTxIn(NullOutPoint, cbase_scriptsig(height), 0xffffffff)]
    coinbaseoutput = CTxOut()
    coinbaseoutput.nValue = cbase_value(height)
    if (pubkey != None):
        coinbaseoutput.scriptPubKey = CScript([pubkey, OP_CHECKSIG])
    else:
        coinbaseoutput.scriptPubKey = CScript([OP_TRUE])
    coinbase.vout = [coinbaseoutput]
    coinbase.calc_sha256()
    return coinbase

def create_coinbase_pos(height):
    coinbase = CTransaction()
    coinbase.vin = [CTxIn(NullOutPoint, cbase_scriptsig(height), 0xffffffff)]
    coinbase.vout = [CTxOut(0, b"")]
    coinbase.calc_sha256()
    return coinbase

# Create a transaction.
# If the scriptPubKey is not specified, make it anyone-can-spend.
def create_transaction(prevtx, n, sig, value, scriptPubKey=CScript()):
    tx = CTransaction()
    assert(n < len(prevtx.vout))
    tx.vin.append(CTxIn(COutPoint(prevtx.sha256, n), sig, 0xffffffff))
    tx.vout.append(CTxOut(value, scriptPubKey))
    tx.calc_sha256()
    return tx

def create_transaction_from_outpoint(outPoint, sig, value, scriptPubKey=CScript()):
    tx = CTransaction()
    tx.vin.append(CTxIn(outPoint, sig, 0xffffffff))
    tx.vout.append(CTxOut(value, scriptPubKey))
    tx.calc_sha256()
    return tx

def get_legacy_sigopcount_block(block, fAccurate=True):
    count = 0
    for tx in block.vtx:
        count += get_legacy_sigopcount_tx(tx, fAccurate)
    return count

def get_legacy_sigopcount_tx(tx, fAccurate=True):
    count = 0
    for i in tx.vout:
        count += i.scriptPubKey.GetSigOpCount(fAccurate)
    for j in tx.vin:
        # scriptSig might be of type bytes, so convert to CScript for the moment
        count += CScript(j.scriptSig).GetSigOpCount(fAccurate)
    return count

### PIVX specific blocktools ###
vZC_DENOMS = [1, 5, 10, 50, 100, 500, 1000, 5000]

def get_prevouts(rpc_conn, utxo_list, zpos=False, nHeight=-1):
    ''' get prevouts (map) for each utxo in a list
    :param   rpc_conn:                  (TestNode) node used as rpc connection
             utxo_list: <if zpos=False> (JSON list) utxos returned from listunspent used as input
                        <if zpos=True>  (JSON list) mints returned from listmintedzerocoins used as input
             zpos:                      (bool) type of utxo_list
             nHeight:                   (int) height of the previous block. used only if zpos=True for
                                        stake checksum. Optional, if not provided rpc_conn's height is used.
    :return: prevouts:         ({bytes --> (int, bytes, int)} dictionary)
                               maps CStake "uniqueness" (i.e. serialized COutPoint -or hash stake, for zpiv-)
                               to (amount, prevScript, timeBlockFrom).
                               For zpiv prevScript is replaced with serialHash hex string.
    '''
    prevouts = {}

    for utxo in utxo_list:
        if not zpos:
            outPoint = COutPoint(int(utxo['txid'], 16), utxo['vout'])
            outValue = int(utxo['amount']) * COIN
            prevtx_json = rpc_conn.getrawtransaction(utxo['txid'], 1)
            prevTx = CTransaction()
            prevTx.deserialize(BytesIO(hex_str_to_bytes(prevtx_json['hex'])))
            if (prevTx.is_coinbase() or prevTx.is_coinstake()) and utxo['confirmations'] < 100:
                # skip immature coins
                continue
            prevScript = prevtx_json['vout'][utxo['vout']]['scriptPubKey']['hex']
            prevTime = prevtx_json['blocktime']
            prevouts[outPoint.serialize_uniqueness()] = (outValue, prevScript, prevTime)

        else:
            # get mint checkpoint
            if nHeight == -1:
                nHeight = rpc_conn.getblockcount()
            checkpointBlock = rpc_conn.getblock(rpc_conn.getblockhash(nHeight), True)
            checkpoint = int(checkpointBlock['acc_checkpoint'], 16)
            # parse checksum and get checksumblock time
            pos = vZC_DENOMS.index(utxo['denomination'])
            checksum = (checkpoint >> (32 * (len(vZC_DENOMS) - 1 - pos))) & 0xFFFFFFFF
            prevTime = rpc_conn.getchecksumblock(hex(checksum), utxo['denomination'], True)['time']
            uniqueness = bytes.fromhex(utxo['hash stake'])[::-1]
            prevouts[uniqueness] = (int(utxo["denomination"]) * COIN, utxo["serial hash"], prevTime)

    return prevouts

def make_txes(rpc_conn, spendingPrevOuts, to_pubKey):
    ''' makes a list of CTransactions each spending an input from spending PrevOuts to an output to_pubKey
    :param   rpc_conn:           (TestNode) node used as rpc connection (must own the prevOuts)
             spendingPrevouts:   ({bytes --> (int, bytes, int)} dictionary)
                                 maps CStake "uniqueness" (i.e. serialized COutPoint -or hash stake, for zpiv-)
                                 to (amount, prevScript, timeBlockFrom).
                                 For zpiv prevScript is replaced with serialHash hex string.
             to_pubKey           (bytes) recipient public key
    :return: block_txes:         ([CTransaction] list)
    '''
    use_fee = 0.0001
    block_txes = []
    for uniqueness in spendingPrevOuts:
        value_out = int(spendingPrevOuts[uniqueness][0] - use_fee * COIN)
        scriptPubKey = CScript([to_pubKey, OP_CHECKSIG])
        prevout = COutPoint()
        prevout.deserialize_uniqueness(BytesIO(uniqueness))
        tx = create_transaction_from_outpoint(prevout, b"", value_out, scriptPubKey)
        # sign tx
        signed_tx_hex = rpc_conn.signrawtransaction(bytes_to_hex_str(tx.serialize()))['hex']
        signed_tx = CTransaction()
        signed_tx.from_hex(signed_tx_hex)
        # add signed tx to the list
        block_txes.append(signed_tx)

    return block_txes

DUMMY_KEY = CECKey()
DUMMY_KEY.set_secretbytes(hash256(pack('<I', 0xffff)))

def stake_block(
        nHeight,
        prevHhash,
        rpc_conn,
        stakeableUtxos,
        startTime=None,
        privKeyWIF=None,
        vtx=[],
        fDoubleSpend=False):
    ''' manually stakes a block selecting the coinstake input from a list of candidates
    :param   nHeight:           (int) height of the block being produced
             prevHash:          (string) hex string of the previous block's hash
             rpc_conn:          (TestNode) node used as rpc connection (must own the stakeableUtxos)
             stakeableUtxos:    ({bytes --> (int, bytes, int)} dictionary)
                                maps CStake "uniqueness" (i.e. serialized COutPoint -or hash stake, for zpiv-)
                                to (amount, prevScript, timeBlockFrom).
                                For zpiv prevScript is replaced with serialHash hex string.
             startTime:         (int) epoch time to be used as blocktime (iterated in solve_stake)
             privKeyWIF:        (string) private key to be used for staking/signing
                                If empty string, it will be used the pk from the stake input
                                (dumping the sk from rpc_conn). If None, then the DUMMY_KEY will be used.
             vtx:               ([CTransaction] list) transactions to add to block.vtx
             fDoubleSpend:      (bool) wether any tx in vtx is allowed to spend the coinstake input
    :return: block:             (CBlock) block produced, must be manually relayed
    '''
    if not len(stakeableUtxos) > 0:
        raise Exception("Need at least one stakeable utxo to stake a block!")
    # Get start time to stake
    if startTime is None:
        startTime = time.time()
    # Create empty block with coinbase
    nTime = int(startTime) & 0xfffffff0
    coinbaseTx = create_coinbase_pos(nHeight)
    block = create_block(int(prevHhash, 16), coinbaseTx, nTime)

    # Find valid kernel hash - iterates stakeableUtxos, then block.nTime
    block.solve_stake(stakeableUtxos)

    # Check if this is a zPoS block or regular/cold stake - sign stake tx
    block_sig_key = CECKey()
    prevout = None
    if len(block.prevoutStake) == 32:
        # zPoS block
        _, serialHash, _ = stakeableUtxos[block.prevoutStake]
        raw_stake = rpc_conn.createrawzerocoinstake(serialHash)
        stake_tx_signed_raw_hex = raw_stake["hex"]
        stake_pkey = raw_stake["private-key"]
        block_sig_key.set_compressed(True)
        block_sig_key.set_secretbytes(bytes.fromhex(stake_pkey))
    elif len(block.prevoutStake) == 36:
        # PoS block
        coinstakeTx_unsigned = CTransaction()
        prevout = COutPoint()
        prevout.deserialize_uniqueness(BytesIO(block.prevoutStake))
        coinstakeTx_unsigned.vin.append(CTxIn(prevout, b"", 0xffffffff))
        coinstakeTx_unsigned.vout.append(CTxOut())
        amount, prevScript, _ = stakeableUtxos[block.prevoutStake]
        outNValue = int(amount + 250 * COIN)
        coinstakeTx_unsigned.vout.append(CTxOut(outNValue, hex_str_to_bytes(prevScript)))
        if privKeyWIF == "":
            # Use dummy key
            block_sig_key = DUMMY_KEY
            # replace coinstake output script
            coinstakeTx_unsigned.vout[1].scriptPubKey = CScript([block_sig_key.get_pubkey(), OP_CHECKSIG])
        else:
            if privKeyWIF == None:
                # Use pk of the input. Ask sk from rpc_conn
                rawtx = rpc_conn.getrawtransaction('{:064x}'.format(prevout.hash), True)
                privKeyWIF = rpc_conn.dumpprivkey(rawtx["vout"][prevout.n]["scriptPubKey"]["addresses"][0])
            # Use the provided privKeyWIF (cold staking).
            # export the corresponding private key to sign block
            privKey, compressed = wif_to_privkey(privKeyWIF)
            block_sig_key.set_compressed(compressed)
            block_sig_key.set_secretbytes(bytes.fromhex(privKey))
        # Sign coinstake TX and add it to the block
        stake_tx_signed_raw_hex = rpc_conn.signrawtransaction(
            bytes_to_hex_str(coinstakeTx_unsigned.serialize()))['hex']
    else:
        raise Exception("Invalid stake uniqueness len: %d" % len(block.prevoutStake))

    # Add coinstake to the block
    coinstakeTx = CTransaction()
    coinstakeTx.from_hex(stake_tx_signed_raw_hex)
    block.vtx.append(coinstakeTx)

    # Add provided transactions to the block.
    # Don't add tx doublespending the coinstake input, unless fDoubleSpend=True
    for tx in vtx:
        # assume txes can't spend zpiv stake inputs (in which case prevout=None)
        if prevout is not None and tx.spends(prevout) and not fDoubleSpend:
            continue
        block.vtx.append(tx)

    # Get correct MerkleRoot and rehash block
    block.hashMerkleRoot = block.calc_merkle_root()
    block.rehash()

    # sign block with block signing key and return it
    block.sign_block(block_sig_key)
    return block

def stake_next_block(
        rpc_conn,
        stakeableUtxos,
        btime=None,
        privKeyWIF=None,
        vtx=[],
        fDoubleSpend=False):
    # Get block height and previous block hash
    nHeight = rpc_conn.getblockcount()
    prevHhash = rpc_conn.getblockhash(nHeight)
    nHeight += 1
    # Stake block
    return stake_block(nHeight, prevHhash,rpc_conn,
        stakeableUtxos, btime, privKeyWIF, vtx, fDoubleSpend)
