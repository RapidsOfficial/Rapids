JSON-RPC API
============

Token Core is a fork of Bitcoin Core, with Token Protocol feature support added as a new layer of functionality on top. As such interacting with the API is done in the same manner (JSON-RPC) as Bitcoin Core, simply with additional RPCs available for utilizing Token Protocol features.

As all existing Bitcoin Core functionality is inherent to Token Core, the RPC port by default remains as `8332` as per Bitcoin Core.  If you wish to run Token Core in tandem with Bitcoin Core (eg. via a separate datadir) you may utilize the `-rpcport<port>` option to nominate an alternative port number.

All available commands can be listed with `"help"`, and information about a specific command can be retrieved with `"help <command>"`.

*Please note: this document may not always be up-to-date. There may be errors, omissions or inaccuracies present.*


## Table of contents

- [Transaction creation](#transaction-creation)
  - [token_send](#token_send)
  - [token_senddexsell](#token_senddexsell)
  - [token_senddexaccept](#token_senddexaccept)
  - [token_sendissuancecrowdsale](#token_sendissuancecrowdsale)
  - [token_sendissuancefixed](#token_sendissuancefixed)
  - [token_sendissuancemanaged](#token_sendissuancemanaged)
  - [token_sendsto](#token_sendsto)
  - [token_sendgrant](#token_sendgrant)
  - [token_sendrevoke](#token_sendrevoke)
  - [token_sendclosecrowdsale](#token_sendclosecrowdsale)
  - [token_sendtrade](#token_sendtrade)
  - [token_sendcanceltradesbyprice](#token_sendcanceltradesbyprice)
  - [token_sendcanceltradesbypair](#token_sendcanceltradesbypair)
  - [token_sendcancelalltrades](#token_sendcancelalltrades)
  - [token_sendchangeissuer](#token_sendchangeissuer)
  - [token_sendall](#token_sendall)
  - [token_sendenablefreezing](#token_sendenablefreezing)
  - [token_senddisablefreezing](#token_senddisablefreezing)
  - [token_sendfreeze](#token_sendfreeze)
  - [token_sendunfreeze](#token_sendunfreeze)
  - [token_sendrawtx](#token_sendrawtx)
  - [token_funded_send](#token_funded_send)
  - [token_funded_sendall](#token_funded_sendall)
- [Data retrieval](#data-retrieval)
  - [token_getinfo](#token_getinfo)
  - [token_getbalance](#token_getbalance)
  - [token_getallbalancesforid](#token_getallbalancesforid)
  - [token_getallbalancesforaddress](#token_getallbalancesforaddress)
  - [token_getwalletbalances](#token_getwalletbalances)
  - [token_getwalletaddressbalances](#token_getwalletaddressbalances)
  - [token_gettransaction](#token_gettransaction)
  - [token_listtransactions](#token_listtransactions)
  - [token_listblocktransactions](#token_listblocktransactions)
  - [token_listblockstransactions](#token_listblockstransactions)
  - [token_listpendingtransactions](#token_listpendingtransactions)
  - [token_getactivedexsells](#token_getactivedexsells)
  - [token_listproperties](#token_listproperties)
  - [token_getproperty](#token_getproperty)
  - [token_getactivecrowdsales](#token_getactivecrowdsales)
  - [token_getcrowdsale](#token_getcrowdsale)
  - [token_getgrants](#token_getgrants)
  - [token_getsto](#token_getsto)
  - [token_gettrade](#token_gettrade)
  - [token_getorderbook](#token_getorderbook)
  - [token_gettradehistoryforpair](#token_gettradehistoryforpair)
  - [token_gettradehistoryforaddress](#token_gettradehistoryforaddress)
  - [token_getactivations](#token_getactivations)
  - [token_getpayload](#token_getpayload)
  - [token_getseedblocks](#token_getseedblocks)
  - [token_getcurrentconsensushash](#token_getcurrentconsensushash)
- [Raw transactions](#raw-transactions)
  - [token_decodetransaction](#token_decodetransaction)
  - [token_createrawtx_opreturn](#token_createrawtx_opreturn)
  - [token_createrawtx_multisig](#token_createrawtx_multisig)
  - [token_createrawtx_input](#token_createrawtx_input)
  - [token_createrawtx_reference](#token_createrawtx_reference)
  - [token_createrawtx_change](#token_createrawtx_change)
  - [token_createpayload_simplesend](#token_createpayload_simplesend)
  - [token_createpayload_sendall](#token_createpayload_sendall)
  - [token_createpayload_dexsell](#token_createpayload_dexsell)
  - [token_createpayload_dexaccept](#token_createpayload_dexaccept)
  - [token_createpayload_sto](#token_createpayload_sto)
  - [token_createpayload_issuancefixed](#token_createpayload_issuancefixed)
  - [token_createpayload_issuancecrowdsale](#token_createpayload_issuancecrowdsale)
  - [token_createpayload_issuancemanaged](#token_createpayload_issuancemanaged)
  - [token_createpayload_closecrowdsale](#token_createpayload_closecrowdsale)
  - [token_createpayload_grant](#token_createpayload_grant)
  - [token_createpayload_revoke](#token_createpayload_revoke)
  - [token_createpayload_changeissuer](#token_createpayload_changeissuer)
  - [token_createpayload_trade](#token_createpayload_trade)
  - [token_createpayload_canceltradesbyprice](#token_createpayload_canceltradesbyprice)
  - [token_createpayload_canceltradesbypair](#token_createpayload_canceltradesbypair)
  - [token_createpayload_cancelalltrades](#token_createpayload_cancelalltrades)
  - [token_createpayload_enablefreezing](#token_createpayload_enablefreezing)
  - [token_createpayload_disablefreezing](#token_createpayload_disablefreezing)
  - [token_createpayload_freeze](#token_createpayload_freeze)
  - [token_createpayload_unfreeze](#token_createpayload_unfreeze)
- [Fee system](#fee-system)
  - [token_getfeecache](#token_getfeecache)
  - [token_getfeetrigger](#token_getfeetrigger)
  - [token_getfeeshare](#token_getfeeshare)
  - [token_getfeedistribution](#token_getfeedistribution)
  - [token_getfeedistributions](#token_getfeedistributions)
- [Configuration](#configuration)
  - [token_setautocommit](#token_setautocommit)
- [Depreciated API calls](#depreciated-api-calls)

---

## Transaction creation

The RPCs for transaction creation can be used to create and broadcast Token Protocol transactions.

A hash of the broadcasted transaction is returned as result.

### token_send

Create and broadcast a simple send transaction.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from                                                                     |
| `toaddress`         | string  | required | the address of the receiver                                                                  |
| `propertyid`        | number  | required | the identifier of the tokens to send                                                         |
| `amount`            | string  | required | the amount to send                                                                           |
| `redeemaddress`     | string  | optional | an address that can spend the transaction dust (sender by default)                           |
| `referenceamount`   | string  | optional | a bitcoin amount that is sent to the receiver (minimal by default)                           |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_send" "3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY" "37FaKponF7zqoMLUjEiko25pDiuVH5YLEa" 1 "100.0"
```

---

### token_senddexsell

Place, update or cancel a sell offer on the traditional distributed TOKEN/BTC exchange.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from                                                                     |
| `propertyidforsale` | number  | required | the identifier of the tokens to list for sale (must be `1` for `OMN` or `2`for `TOMN`)       |
| `amountforsale`     | string  | required | the amount of tokens to list for sale                                                        |
| `amountdesired`     | string  | required | the amount of bitcoins desired                                                               |
| `paymentwindow`     | number  | required | a time limit in blocks a buyer has to pay following a successful accepting order             |
| `minacceptfee`      | string  | required | a minimum mining fee a buyer has to pay to accept the offer                                  |
| `action`            | number  | required | the action to take (`1` for new offers, `2` to update, `3` to cancel)                        |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_senddexsell" "37FaKponF7zqoMLUjEiko25pDiuVH5YLEa" 1 "1.5" "0.75" 25 "0.0005" 1
```

---

### token_senddexaccept

Create and broadcast an accept offer for the specified token and amount.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from                                                                     |
| `toaddress`         | string  | required | the address of the seller                                                                    |
| `propertyid`        | number  | required | the identifier of the token to purchase                                                      |
| `amount`            | string  | required | the amount to accept                                                                         |
| `override`          | boolean | required | override minimum accept fee and payment window checks (use with caution!)                    |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_senddexaccept" \
    "35URq1NN3xL6GeRKUP6vzaQVcxoJiiJKd8" "37FaKponF7zqoMLUjEiko25pDiuVH5YLEa" 1 "15.0"
```

---

### token_sendissuancecrowdsale

Create new tokens as crowdsale.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from                                                                     |
| `ecosystem`         | number  | required | the ecosystem to create the tokens in (`1` for main ecosystem, `2` for test ecosystem)       |
| `type`              | number  | required | the type of the tokens to create: (`1` for indivisible tokens, `2` for divisible tokens)     |
| `previousid`        | number  | required | an identifier of a predecessor token (`0` for new crowdsales)                                |
| `category`          | string  | required | a category for the new tokens (can be `""`)                                                  |
| `subcategory`       | string  | required | a subcategory for the new tokens (can be `""`)                                               |
| `name`              | string  | required | the name of the new tokens to create                                                         |
| `url`               | string  | required | an URL for further information about the new tokens (can be `""`)                            |
| `data`              | string  | required | a description for the new tokens (can be `""`)                                               |
| `propertyiddesired` | number  | required | the identifier of a token eligible to participate in the crowdsale                           |
| `tokensperunit`     | string  | required | the amount of tokens granted per unit invested in the crowdsale                              |
| `deadline`          | number  | required | the deadline of the crowdsale as Unix timestamp                                              |
| `earlybonus`        | number  | required | an early bird bonus for participants in percent per week                                     |
| `issuerpercentage`  | number  | required | a percentage of tokens that will be granted to the issuer                                    |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendissuancecrowdsale" \
    "3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo" 2 1 0 "Companies" "Bitcoin Mining" \
    "Quantum Miner" "" "" 2 "100" 1483228800 30 2
```

---

### token_sendissuancefixed

Create new tokens with fixed supply.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from                                                                     |
| `ecosystem`         | number  | required | the ecosystem to create the tokens in (`1` for main ecosystem, `2` for test ecosystem)       |
| `type`              | number  | required | the type of the tokens to create: (`1` for indivisible tokens, `2` for divisible tokens)     |
| `previousid`        | number  | required | an identifier of a predecessor token (`0` for new tokens)                                    |
| `category`          | string  | required | a category for the new tokens (can be `""`)                                                  |
| `subcategory`       | string  | required | a subcategory for the new tokens (can be `""`)                                               |
| `name`              | string  | required | the name of the new tokens to create                                                         |
| `url`               | string  | required | an URL for further information about the new tokens (can be `""`)                            |
| `data`              | string  | required | a description for the new tokens (can be `""`)                                               |
| `amount`            | string  | required | the number of tokens to create                                                               |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendissuancefixed" \
    "3Ck2kEGLJtZw9ENj2tameMCtS3HB7uRar3" 2 1 0 "Companies" "Bitcoin Mining" \
    "Quantum Miner" "" "" "1000000"
```

---

### token_sendissuancemanaged

Create new tokens with manageable supply.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from                                                                     |
| `ecosystem`         | number  | required | the ecosystem to create the tokens in (`1` for main ecosystem, `2` for test ecosystem)       |
| `type`              | number  | required | the type of the tokens to create: (`1` for indivisible tokens, `2` for divisible tokens)     |
| `previousid`        | number  | required | an identifier of a predecessor token (`0` for new tokens)                                    |
| `category`          | string  | required | a category for the new tokens (can be `""`)                                                  |
| `subcategory`       | string  | required | a subcategory for the new tokens (can be `""`)                                               |
| `name`              | string  | required | the name of the new tokens to create                                                         |
| `url`               | string  | required | an URL for further information about the new tokens (can be `""`)                            |
| `data`              | string  | required | a description for the new tokens (can be `""`)                                               |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendissuancemanaged" \
    "3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH" 2 1 0 "Companies" "Bitcoin Mining" "Quantum Miner" "" ""
```

---

### token_sendsto

Create and broadcast a send-to-owners transaction.

**Arguments:**

| Name                   | Type    | Presence | Description                                                                                  |
|------------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`          | string  | required | the address to send from                                                                     |
| `propertyid`           | number  | required | the identifier of the tokens to distribute                                                   |
| `amount`               | string  | required | the amount to distribute                                                                     |
| `redeemaddress`        | string  | optional | an address that can spend the transaction dust (sender by default)                           |
| `distributionproperty` | number  | optional | the identifier of the property holders to distribute to                                      |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendsto" \
    "32Z3tJccZuqQZ4PhJR2hxHC3tjgjA8cbqz" "37FaKponF7zqoMLUjEiko25pDiuVH5YLEa" 3 "5000"
```

---

### token_sendgrant

Issue or grant new units of managed tokens.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from                                                                     |
| `toaddress`         | string  | required | the receiver of the tokens (sender by default, can be `""`)                                  |
| `propertyid`        | number  | required | the identifier of the tokens to grant                                                        |
| `amount`            | string  | required | the amount of tokens to create                                                               |
| `memo`              | string  | optional | a text note attached to this transaction (none by default)                                   |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendgrant" "3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH" "" 51 "7000"
```

---

### token_sendrevoke

Revoke units of managed tokens.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from                                                                     |
| `propertyid`        | number  | required | the identifier of the tokens to revoke                                                       |
| `amount`            | string  | required | the amount of tokens to revoke                                                               |
| `memo`              | string  | optional | a text note attached to this transaction (none by default)                                   |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendrevoke" "3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH" "" 51 "100"
```

---

### token_sendclosecrowdsale

Manually close a crowdsale.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address associated with the crowdsale to close                                           |
| `propertyid`        | number  | required | the identifier of the crowdsale to close                                                     |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendclosecrowdsale" "3JYd75REX3HXn1vAU83YuGfmiPXW7BpYXo" 70
```

---

### token_sendtrade

Place a trade offer on the distributed token exchange.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to trade with                                                                    |
| `propertyidforsale` | number  | required | the identifier of the tokens to list for sale                                                |
| `amountforsale`     | string  | required | the amount of tokens to list for sale                                                        |
| `propertiddesired`  | number  | required | the identifier of the tokens desired in exchange                                             |
| `amountdesired`     | string  | required | the amount of tokens desired in exchange                                                     |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendtrade" "3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR" 31 "250.0" 1 "10.0"
```

---

### token_sendcanceltradesbyprice

Cancel offers on the distributed token exchange with the specified price.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to trade with                                                                    |
| `propertyidforsale` | number  | required | the identifier of the tokens listed for sale                                                 |
| `amountforsale`     | string  | required | the amount of tokens to listed for sale                                                      |
| `propertiddesired`  | number  | required | the identifier of the tokens desired in exchange                                             |
| `amountdesired`     | string  | required | the amount of tokens desired in exchange                                                     |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendcanceltradesbyprice" "3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR" 31 "100.0" 1 "5.0"
```

---

### token_sendcanceltradesbypair

Cancel all offers on the distributed token exchange with the given currency pair.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to trade with                                                                    |
| `propertyidforsale` | number  | required | the identifier of the tokens listed for sale                                                 |
| `propertiddesired`  | number  | required | the identifier of the tokens desired in exchange                                             |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendcanceltradesbypair" "3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR" 1 31
```

---

### token_sendcancelalltrades

Cancel all offers on the distributed token exchange.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to trade with                                                                    |
| `ecosystem`         | number  | required | the ecosystem of the offers to cancel (`1` for main ecosystem, `2` for test ecosystem)       |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendcancelalltrades" "3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR" 1
```

---

### token_sendchangeissuer

Change the issuer on record of the given tokens.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address associated with the tokens                                                       |
| `toaddress  `       | string  | required | the address to transfer administrative control to                                            |
| `propertyid`        | number  | required | the identifier of the tokens                                                                 |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendchangeissuer" \
    "1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu" "3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs" 3
```

---

### token_sendall

Transfers all available tokens in the given ecosystem to the recipient.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from                                                                     |
| `toaddress  `       | string  | required | the address of the receiver                                                                  |
| `ecosystem`         | number  | required | the ecosystem of the tokens to send (`1` for main ecosystem, `2` for test ecosystem)         |
| `redeemaddress`     | string  | optional | an address that can spend the transaction dust (sender by default)                           |
| `referenceamount`   | string  | optional | a bitcoin amount that is sent to the receiver (minimal by default)                           |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendall" "3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY" "37FaKponF7zqoMLUjEiko25pDiuVH5YLEa" 2
```

---

### token_sendenablefreezing

Enables address freezing for a centrally managed property.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from (must be issuer of a managed property)                              |
| `propertyid`        | number  | required | the identifier of the tokens                                                                 |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendenablefreezing" "3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY" 2
```

---

### token_senddisablefreezing

Disables address freezing for a centrally managed property.

IMPORTANT NOTE:  Disabling freezing for a property will UNFREEZE all frozen addresses for that property!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from (must be issuer of a managed property)                              |
| `propertyid`        | number  | required | the identifier of the tokens                                                                 |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_senddisablefreezing" "3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY" 2
```

---

### token_sendfreeze

Freeze an address for a centrally managed token.

Note: Only the issuer may freeze tokens, and only if the token is of the managed type with the freezing option enabled.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from (must be issuer of a managed property with freezing enabled         |
| `toaddress`         | string  | required | the address to freeze                                                                        |
| `propertyid`        | number  | required | the identifier of the tokens to freeze                                                       |
| `amount`            | string  | required | the amount to freeze (note: currently unused, frozen addresses cannot transact the property) |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendfreeze" "3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY" "3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs" 2 1000
```

---

### token_sendunfreeze

Unfreeze an address for a centrally managed token.

Note: Only the issuer may unfreeze tokens

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from (must be issuer of a managed property with freezing enabled         |
| `toaddress`         | string  | required | the address to unfreeze                                                                      |
| `propertyid`        | number  | required | the identifier of the tokens to unfreeze                                                     |
| `amount`            | string  | required | the amount to unfreeze (note: currently unused                                               |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendunfreeze" "3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY" "3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs" 2 1000
```

---

### token_sendrawtx

Broadcasts a raw Token Layer transaction.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send from                                                                     |
| `rawtransaction`    | string  | required | the hex-encoded raw transaction                                                              |
| `referenceaddress`  | string  | optional | a reference address (none by default)                                                        |
| `redeemaddress`     | string  | optional | an address that can spend the transaction dust (sender by default)                           |
| `referenceamount`   | string  | optional | a bitcoin amount that is sent to the receiver (minimal by default)                           |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_sendrawtx" \
    "1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8" "000000000000000100000000017d7840" \
    "1EqTta1Rt8ixAA32DuC29oukbsSWU62qAV"
```

---

### token_funded_send

Creates and sends a funded simple send transaction.

All bitcoins from the sender are consumed and if there are bitcoins missing, they are taken from the specified fee source. Change is sent to the fee source!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send the tokens from                                                          |
| `toaddress`         | string  | required | the address of the receiver                                                                  |
| `propertyid`        | number  | required | the identifier of the tokens to send                                                         |
| `amount`            | string  | required | the amount to send                                                                           |
| `feeaddress`        | string  | required | the address that is used for change and to pay for fees, if needed                           |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_funded_send" "1DFa5bT6KMEr6ta29QJouainsjaNBsJQhH" \
    "15cWrfuvMxyxGst2FisrQcvcpF48x6sXoH" 1 "100.0" \
    "15Jhzz4omEXEyFKbdcccJwuVPea5LqsKM1"
```

---

### token_funded_sendall

Creates and sends a transaction that transfers all available tokens in the given ecosystem to the recipient.

All bitcoins from the sender are consumed and if there are bitcoins missing, they are taken from the specified fee source. Change is sent to the fee source!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `fromaddress`       | string  | required | the address to send the tokens from                                                          |
| `toaddress`         | string  | required | the address of the receiver                                                                  |
| `ecosystem`         | number  | required | the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)             |
| `feeaddress`        | string  | required | the address that is used for change and to pay for fees, if needed                           |

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ tokencore-cli "token_funded_sendall" "1DFa5bT6KMEr6ta29QJouainsjaNBsJQhH" \
    "15cWrfuvMxyxGst2FisrQcvcpF48x6sXoH" 1 "15Jhzz4omEXEyFKbdcccJwuVPea5LqsKM1"
```

---


## Data retrieval

The RPCs for data retrieval can be used to get information about the state of the Token ecosystem.

### token_getinfo

Returns various state information of the client and protocol.

**Arguments:**

*None*

**Result:**
```js
Result:
{
  "tokencoreversion_int" : xxxxxxx,      // (number) client version as integer
  "tokencoreversion" : "x.x.x.x-xxx",    // (string) client version
  "mastercoreversion" : "x.x.x.x-xxx",  // (string) client version (DEPRECIATED)
  "bitcoincoreversion" : "x.x.x",       // (string) Bitcoin Core version
  "commitinfo" : "xxxxxxx",             // (string) build commit identifier
  "block" : nnnnnn,                     // (number) index of the last processed block
  "blocktime" : nnnnnnnnnn,             // (number) timestamp of the last processed block
  "blocktransactions" : nnnn,           // (number) Token transactions found in the last processed block
  "totaltransactions" : nnnnnnnn,       // (number) Token transactions processed in total
  "alerts" : [                          // (array of JSON objects) active protocol alert (if any)
    {
      "alerttype" : n                       // (number) alert type as integer
      "alerttype" : "xxx"                   // (string) alert type (can be "alertexpiringbyblock", "alertexpiringbyblocktime", "alertexpiringbyclientversion" or "error")
      "alertexpiry" : "nnnnnnnnnn"          // (string) expiration criteria (can refer to block height, timestamp or client verion)
      "alertmessage" : "xxx"                // (string) information about the alert
    },
    ...
  ]
}
```

**Example:**

```bash
$ tokencore-cli "token_getinfo"
```

---

### token_getbalance

Returns the token balance for a given address and property.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `address`           | string  | required | the address                                                                                  |
| `propertyid`        | number  | required | the property identifier                                                                      |

**Result:**
```js
{
  "balance" : "n.nnnnnnnn",  // (string) the available balance of the address
  "reserved" : "n.nnnnnnnn", // (string) the amount reserved by sell offers and accepts
  "frozen" : "n.nnnnnnnn"    // (string) the amount frozen by the issuer (applies to managed properties only)
}
```

**Example:**

```bash
$ tokencore-cli "token_getbalance", "1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P" 1
```

---

### token_getallbalancesforid

Returns a list of token balances for a given currency or property identifier.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the property identifier                                                                      |

**Result:**
```js
[                          // (array of JSON objects)
  {
    "address" : "address",     // (string) the address
    "balance" : "n.nnnnnnnn",  // (string) the available balance of the address
    "reserved" : "n.nnnnnnnn", // (string) the amount reserved by sell offers and accepts
    "frozen" : "n.nnnnnnnn"    // (string) the amount frozen by the issuer (applies to managed properties only)
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_getallbalancesforid" 1
```

---

### token_getallbalancesforaddress

Returns a list of all token balances for a given address.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `address`           | string  | required | the address                                                                                  |

**Result:**
```js
[                          // (array of JSON objects)
  {
    "propertyid" : n,          // (number) the property identifier
    "name" : "name",           // (string) the name of the property
    "balance" : "n.nnnnnnnn",  // (string) the available balance of the address
    "reserved" : "n.nnnnnnnn", // (string) the amount reserved by sell offers and accepts
    "frozen" : "n.nnnnnnnn"    // (string) the amount frozen by the issuer (applies to managed properties only)
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_getallbalancesforaddress" "1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P"
```

---

### token_getwalletbalances

Returns a list of the total token balances of the whole wallet.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `includewatchonly`  | boolean | optional | include balances of watchonly addresses (default: false)                                     |

**Result:**
```js
[                           // (array of JSON objects)
  {
    "propertyid" : n,         // (number) the property identifier
    "name" : "name",            // (string) the name of the token
    "balance" : "n.nnnnnnnn",   // (string) the total available balance for the token
    "reserved" : "n.nnnnnnnn"   // (string) the total amount reserved by sell offers and accepts
    "frozen" : "n.nnnnnnnn"     // (string) the total amount frozen by the issuer (applies to managed properties only)
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_getwalletbalances"
```

---

### token_getwalletaddressbalances

Returns a list of all token balances for every wallet address.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `includewatchonly`  | boolean | optional | include balances of watchonly addresses (default: false)                                     |

**Result:**
```js
[                           // (array of JSON objects)
  {
    "address" : "address",      // (string) the address linked to the following balances
    "balances" :
    [
      {
        "propertyid" : n,         // (number) the property identifier
        "name" : "name",            // (string) the name of the token
        "balance" : "n.nnnnnnnn",   // (string) the available balance for the token
        "reserved" : "n.nnnnnnnn"   // (string) the amount reserved by sell offers and accepts
        "frozen" : "n.nnnnnnnn"     // (string) the amount frozen by the issuer (applies to managed properties only)
      },
      ...
    ]
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_getwalletaddressbalances"
```

---

### token_gettransaction

Get detailed information about an Token transaction.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `txid`              | string  | required | the hash of the transaction to lookup                                                        |

**Result:**
```js
{
  "txid" : "hash",                 // (string) the hex-encoded hash of the transaction
  "sendingaddress" : "address",    // (string) the Bitcoin address of the sender
  "referenceaddress" : "address",  // (string) a Bitcoin address used as reference (if any)
  "ismine" : true|false,           // (boolean) whether the transaction involes an address in the wallet
  "confirmations" : nnnnnnnnnn,    // (number) the number of transaction confirmations
  "fee" : "n.nnnnnnnn",            // (string) the transaction fee in bitcoins
  "blocktime" : nnnnnnnnnn,        // (number) the timestamp of the block that contains the transaction
  "valid" : true|false,            // (boolean) whether the transaction is valid
  "positioninblock" : n,           // (number) the position (index) of the transaction within the block
  "version" : n,                   // (number) the transaction version
  "type_int" : n,                  // (number) the transaction type as number
  "type" : "type",                 // (string) the transaction type as string
  [...]                            // (mixed) other transaction type specific properties
}
```

**Example:**

```bash
$ tokencore-cli "token_gettransaction" "1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d"
```

---

### token_listtransactions

List wallet transactions, optionally filtered by an address and block boundaries.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `txid`              | string  | optional | address filter (default: `"*"`)                                                              |
| `count`             | number  | optional | show at most n transactions (default: `10`)                                                  |
| `skip`              | number  | optional | skip the first n transactions (default: `0`)                                                 |
| `startblock`        | number  | optional | first block to begin the search (default: `0`)                                               |
| `endblock`          | number  | optional | last block to include in the search (default: `999999999`)                                   |

**Result:**
```js
[                                // (array of JSON objects)
  {
    "txid" : "hash",                 // (string) the hex-encoded hash of the transaction
    "sendingaddress" : "address",    // (string) the Bitcoin address of the sender
    "referenceaddress" : "address",  // (string) a Bitcoin address used as reference (if any)
    "ismine" : true|false,           // (boolean) whether the transaction involves an address in the wallet
    "confirmations" : nnnnnnnnnn,    // (number) the number of transaction confirmations
    "fee" : "n.nnnnnnnn",            // (string) the transaction fee in bitcoins
    "blocktime" : nnnnnnnnnn,        // (number) the timestamp of the block that contains the transaction
    "valid" : true|false,            // (boolean) whether the transaction is valid
    "positioninblock" : n,           // (number) the position (index) of the transaction within the block
    "version" : n,                   // (number) the transaction version
    "type_int" : n,                  // (number) the transaction type as number
    "type" : "type",                 // (string) the transaction type as string
    [...]                            // (mixed) other transaction type specific properties
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_listtransactions"
```

---

### token_listblocktransactions

Lists all Token transactions in a block.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `index`             | number  | required | the block height or block index                                                              |

**Result:**
```js
[      // (array of string)
  "hash",  // (string) the hash of the transaction
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_listblocktransactions" 279007
```

---

### token_listblockstransactions

Lists all Token transactions in a given range of blocks.

Note: the list of transactions is unordered and can contain invalid transactions!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `firstblock`        | number  | required | the index of the first block to consider                                                     |
| `lastblock`         | number  | required | the index of the last block to consider                                                      |

**Result:**
```js
[      // (array of string)
  "hash",  // (string) the hash of the transaction
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_token_listblocktransactions" 279007 300000
```

---

### token_listpendingtransactions

Returns a list of unconfirmed Token transactions, pending in the memory pool.

Note: the validity of pending transactions is uncertain, and the state of the memory pool may change at any moment. It is recommended to check transactions after confirmation, and pending transactions should be considered as invalid.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `address`           | string  | optional | filter results by address (default: `""` for no filter)                                      |

**Result:**
```js
[                                // (array of JSON objects)
  {
    "txid" : "hash",                 // (string) the hex-encoded hash of the transaction
    "sendingaddress" : "address",    // (string) the Bitcoin address of the sender
    "referenceaddress" : "address",  // (string) a Bitcoin address used as reference (if any)
    "ismine" : true|false,           // (boolean) whether the transaction involes an address in the wallet
    "fee" : "n.nnnnnnnn",            // (string) the transaction fee in bitcoins
    "version" : n,                   // (number) the transaction version
    "type_int" : n,                  // (number) the transaction type as number
    "type" : "type",                 // (string) the transaction type as string
    [...]                            // (mixed) other transaction type specific properties
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_listpendingtransactions"
```

---

### token_getactivedexsells

Returns currently active offers on the distributed exchange.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `address`           | string  | optional | address filter (default: include any)                                                        |

**Result:**
```js
[                                  // (array of JSON objects)
  {
    "txid" : "hash",                   // (string) the hash of the transaction of this offer
    "propertyid" : n,                  // (number) the identifier of the tokens for sale
    "seller" : "address",              // (string) the Bitcoin address of the seller
    "amountavailable" : "n.nnnnnnnn",  // (string) the number of tokens still listed for sale and currently available
    "bitcoindesired" : "n.nnnnnnnn",   // (string) the number of bitcoins desired in exchange
    "unitprice" : "n.nnnnnnnn" ,       // (string) the unit price (BTC/token)
    "timelimit" : nn,                  // (number) the time limit in blocks a buyer has to pay following a successful accept
    "minimumfee" : "n.nnnnnnnn",       // (string) the minimum mining fee a buyer has to pay to accept this offer
    "amountaccepted" : "n.nnnnnnnn",   // (string) the number of tokens currently reserved for pending "accept" orders
    "accepts": [                       // (array of JSON objects) a list of pending "accept" orders
      {
        "buyer" : "address",               // (string) the Bitcoin address of the buyer
        "block" : nnnnnn,                  // (number) the index of the block that contains the "accept" order
        "blocksleft" : nn,                 // (number) the number of blocks left to pay
        "amount" : "n.nnnnnnnn"            // (string) the amount of tokens accepted and reserved
        "amounttopay" : "n.nnnnnnnn"       // (string) the amount in bitcoins needed finalize the trade
      },
      ...
    ]
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_getactivedexsells"
```

---

### token_listproperties

Lists all tokens or smart properties.

**Arguments:**

*None*

**Result:**
```js
[                               // (array of JSON objects)
  {
    "propertyid" : n,               // (number) the identifier of the tokens
    "name" : "name",                // (string) the name of the tokens
    "category" : "category",        // (string) the category used for the tokens
    "subcategory" : "subcategory",  // (string) the subcategory used for the tokens
    "data" : "information",         // (string) additional information or a description
    "url" : "uri",                  // (string) an URI, for example pointing to a website
    "divisible" : true|false        // (boolean) whether the tokens are divisible
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_listproperties"
```

---

### token_getproperty

Returns details for about the tokens or smart property to lookup.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the tokens or property                                                     |

**Result:**
```js
{
  "propertyid" : n,               // (number) the identifier
  "name" : "name",                // (string) the name of the tokens
  "category" : "category",        // (string) the category used for the tokens
  "subcategory" : "subcategory",  // (string) the subcategory used for the tokens
  "data" : "information",         // (string) additional information or a description
  "url" : "uri",                  // (string) an URI, for example pointing to a website
  "divisible" : true|false,       // (boolean) whether the tokens are divisible
  "issuer" : "address",           // (string) the Bitcoin address of the issuer on record
  "creationtxid" : "hash",        // (string) the hex-encoded creation transaction hash
  "fixedissuance" : true|false,   // (boolean) whether the token supply is fixed
  "managedissuance" : true|false, // (boolean) whether the token supply is managed by the issuer
  "freezingenabled" : true|false, // (boolean) whether freezing is enabled for the property (managed properties only)
  "totaltokens" : "n.nnnnnnnn"    // (string) the total number of tokens in existence
}
```

**Example:**

```bash
$ tokencore-cli "token_getproperty" 3
```

---

### token_getactivecrowdsales

Lists currently active crowdsales.

**Arguments:**

*None*

**Result:**
```js
[                                // (array of JSON objects)
  {
    "propertyid" : n,                // (number) the identifier of the crowdsale
    "name" : "name",                 // (string) the name of the tokens issued via the crowdsale
    "issuer" : "address",            // (string) the Bitcoin address of the issuer on record
    "propertyiddesired" : n,         // (number) the identifier of the tokens eligible to participate in the crowdsale
    "tokensperunit" : "n.nnnnnnnn",  // (string) the amount of tokens granted per unit invested in the crowdsale
    "earlybonus" : n,                // (number) an early bird bonus for participants in percent per week
    "percenttoissuer" : n,           // (number) a percentage of tokens that will be granted to the issuer
    "starttime" : nnnnnnnnnn,        // (number) the start time of the of the crowdsale as Unix timestamp
    "deadline" : nnnnnnnnnn          // (number) the deadline of the crowdsale as Unix timestamp
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_getactivecrowdsales"
```

---

### token_getcrowdsale

Returns information about a crowdsale.

**Arguments:**,

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the crowdsale                                                              |
| `verbose`           | boolean | optional | list crowdsale participants (default: `false`)                                               |

**Result:**
```js
{
  "propertyid" : n,                    // (number) the identifier of the crowdsale
  "name" : "name",                     // (string) the name of the tokens issued via the crowdsale
  "active" : true|false,               // (boolean) whether the crowdsale is still active
  "issuer" : "address",                // (string) the Bitcoin address of the issuer on record
  "propertyiddesired" : n,             // (number) the identifier of the tokens eligible to participate in the crowdsale
  "tokensperunit" : "n.nnnnnnnn",      // (string) the amount of tokens granted per unit invested in the crowdsale
  "earlybonus" : n,                    // (number) an early bird bonus for participants in percent per week
  "percenttoissuer" : n,               // (number) a percentage of tokens that will be granted to the issuer
  "starttime" : nnnnnnnnnn,            // (number) the start time of the of the crowdsale as Unix timestamp
  "deadline" : nnnnnnnnnn,             // (number) the deadline of the crowdsale as Unix timestamp
  "amountraised" : "n.nnnnnnnn",       // (string) the amount of tokens invested by participants
  "tokensissued" : "n.nnnnnnnn",       // (string) the total number of tokens issued via the crowdsale
  "issuerbonustokens" : "n.nnnnnnnn",  // (string) the amount of tokens granted to the issuer as bonus
  "addedissuertokens" : "n.nnnnnnnn",  // (string) the amount of issuer bonus tokens not yet emitted
  "closedearly" : true|false,          // (boolean) whether the crowdsale ended early (if not active)
  "maxtokens" : true|false,            // (boolean) whether the crowdsale ended early due to reaching the limit of max. issuable tokens (if not active)
  "endedtime" : nnnnnnnnnn,            // (number) the time when the crowdsale ended (if closed early)
  "closetx" : "hash",                  // (string) the hex-encoded hash of the transaction that closed the crowdsale (if closed manually)
  "participanttransactions": [         // (array of JSON objects) a list of crowdsale participations (if verbose=true)
    {
      "txid" : "hash",                     // (string) the hex-encoded hash of participation transaction
      "amountsent" : "n.nnnnnnnn",         // (string) the amount of tokens invested by the participant
      "participanttokens" : "n.nnnnnnnn",  // (string) the tokens granted to the participant
      "issuertokens" : "n.nnnnnnnn"        // (string) the tokens granted to the issuer as bonus
    },
    ...
  ]
}
```

**Example:**

```bash
$ tokencore-cli "token_getcrowdsale" 3 true
```

---

### token_getgrants

Returns information about granted and revoked units of managed tokens.

**Arguments:**

*None*

**Result:**
```js
{
  "propertyid" : n,              // (number) the identifier of the managed tokens
  "name" : "name",               // (string) the name of the tokens
  "issuer" : "address",          // (string) the Bitcoin address of the issuer on record
  "creationtxid" : "hash",       // (string) the hex-encoded creation transaction hash
  "totaltokens" : "n.nnnnnnnn",  // (string) the total number of tokens in existence
  "issuances": [                 // (array of JSON objects) a list of the granted and revoked tokens
    {
      "txid" : "hash",               // (string) the hash of the transaction that granted tokens
      "grant" : "n.nnnnnnnn"         // (string) the number of tokens granted by this transaction
    },
    {
      "txid" : "hash",               // (string) the hash of the transaction that revoked tokens
      "grant" : "n.nnnnnnnn"         // (string) the number of tokens revoked by this transaction
    },
    ...
  ]
}
```

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the managed tokens to lookup                                               |

**Example:**

```bash
$ tokencore-cli "token_getgrants" 31
```

---

### token_getsto

Get information and recipients of a send-to-owners transaction.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `txid`              | string  | required | the hash of the transaction to lookup                                                        |
| `recipientfilter`   | string  | optional | a filter for recipients (wallet by default, `"*"` for all)                                   |

**Result:**
```js
{
  "txid" : "hash",               // (string) the hex-encoded hash of the transaction
  "sendingaddress" : "address",  // (string) the Bitcoin address of the sender
  "ismine" : true|false,         // (boolean) whether the transaction involes an address in the wallet
  "confirmations" : nnnnnnnnnn,  // (number) the number of transaction confirmations
  "fee" : "n.nnnnnnnn",          // (string) the transaction fee in bitcoins
  "blocktime" : nnnnnnnnnn,      // (number) the timestamp of the block that contains the transaction
  "valid" : true|false,          // (boolean) whether the transaction is valid
  "positioninblock" : n,         // (number) the position (index) of the transaction within the block
  "version" : n,                 // (number) the transaction version
  "type_int" : n,                // (number) the transaction type as number
  "type" : "type",               // (string) the transaction type as string
  "propertyid" : n,              // (number) the identifier of sent tokens
  "divisible" : true|false,      // (boolean) whether the sent tokens are divisible
  "amount" : "n.nnnnnnnn",       // (string) the number of tokens sent to owners
  "totalstofee" : "n.nnnnnnnn",  // (string) the fee paid by the sender, nominated in OMN or TOMN
  "recipients": [                // (array of JSON objects) a list of recipients
    {
      "address" : "address",         // (string) the Bitcoin address of the recipient
      "amount" : "n.nnnnnnnn"        // (string) the number of tokens sent to this recipient
    },
    ...
  ]
}
```

**Example:**

```bash
$ tokencore-cli "token_getsto" "1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d" "*"
```

---

### token_gettrade

Get detailed information and trade matches for orders on the distributed token exchange.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `txid`              | string  | required | the hash of the order to lookup                                                              |

**Result:**
```js
{
  "txid" : "hash",                              // (string) the hex-encoded hash of the transaction of the order
  "sendingaddress" : "address",                 // (string) the Bitcoin address of the trader
  "ismine" : true|false,                        // (boolean) whether the order involes an address in the wallet
  "confirmations" : nnnnnnnnnn,                 // (number) the number of transaction confirmations
  "fee" : "n.nnnnnnnn",                         // (string) the transaction fee in bitcoins
  "blocktime" : nnnnnnnnnn,                     // (number) the timestamp of the block that contains the transaction
  "valid" : true|false,                         // (boolean) whether the transaction is valid
  "positioninblock" : n,                        // (number) the position (index) of the transaction within the block
  "version" : n,                                // (number) the transaction version
  "type_int" : n,                               // (number) the transaction type as number
  "type" : "type",                              // (string) the transaction type as string
  "propertyidforsale" : n,                      // (number) the identifier of the tokens put up for sale
  "propertyidforsaleisdivisible" : true|false,  // (boolean) whether the tokens for sale are divisible
  "amountforsale" : "n.nnnnnnnn",               // (string) the amount of tokens initially offered
  "propertyiddesired" : n,                      // (number) the identifier of the tokens desired in exchange
  "propertyiddesiredisdivisible" : true|false,  // (boolean) whether the desired tokens are divisible
  "amountdesired" : "n.nnnnnnnn",               // (string) the amount of tokens initially desired
  "unitprice" : "n.nnnnnnnnnnn..."              // (string) the unit price (shown in the property desired)
  "status" : "status"                           // (string) the status of the order ("open", "cancelled", "filled", ...)
  "canceltxid" : "hash",                        // (string) the hash of the transaction that cancelled the order (if cancelled)
  "matches": [                                  // (array of JSON objects) a list of matched orders and executed trades
    {
      "txid" : "hash",                              // (string) the hash of the transaction that was matched against
      "block" : nnnnnn,                             // (number) the index of the block that contains this transaction
      "address" : "address",                        // (string) the Bitcoin address of the other trader
      "amountsold" : "n.nnnnnnnn",                  // (string) the number of tokens sold in this trade
      "amountreceived" : "n.nnnnnnnn"               // (string) the number of tokens traded in exchange
    },
    ...
  ]
}
```

**Example:**

```bash
$ tokencore-cli "token_gettrade" "1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d"
```

---

### token_getorderbook

List active offers on the distributed token exchange.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | filter orders by `propertyid` for sale                                                       |
| `propertyid`        | number  | optional | filter orders by `propertyid` desired                                                        |

**Result:**
```js
[                                             // (array of JSON objects)
  {
    "address" : "address",                        // (string) the Bitcoin address of the trader
    "txid" : "hash",                              // (string) the hex-encoded hash of the transaction of the order
    "ecosystem" : "main"|"test",                  // (string) the ecosytem in which the order was made (if "cancel-ecosystem")
    "propertyidforsale" : n,                      // (number) the identifier of the tokens put up for sale
    "propertyidforsaleisdivisible" : true|false,  // (boolean) whether the tokens for sale are divisible
    "amountforsale" : "n.nnnnnnnn",               // (string) the amount of tokens initially offered
    "amountremaining" : "n.nnnnnnnn",             // (string) the amount of tokens still up for sale
    "propertyiddesired" : n,                      // (number) the identifier of the tokens desired in exchange
    "propertyiddesiredisdivisible" : true|false,  // (boolean) whether the desired tokens are divisible
    "amountdesired" : "n.nnnnnnnn",               // (string) the amount of tokens initially desired
    "amounttofill" : "n.nnnnnnnn",                // (string) the amount of tokens still needed to fill the offer completely
    "action" : n,                                 // (number) the action of the transaction: (1) "trade", (2) "cancel-price", (3) "cancel-pair", (4) "cancel-ecosystem"
    "block" : nnnnnn,                             // (number) the index of the block that contains the transaction
    "blocktime" : nnnnnnnnnn                      // (number) the timestamp of the block that contains the transaction
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_getorderbook" 2
```

---

### token_gettradehistoryforpair

Retrieves the history of trades on the distributed token exchange for the specified market.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the first side of the traded pair                                                            |
| `propertyid`        | number  | required | the second side of the traded pair                                                           |
| `count`             | number  | optional | number of trades to retrieve (default: `10`)                                                 |

**Result:**
```js
[                                     // (array of JSON objects)
  {
    "block" : nnnnnn,                     // (number) the index of the block that contains the trade match
    "unitprice" : "n.nnnnnnnnnnn..." ,    // (string) the unit price used to execute this trade (received/sold)
    "inverseprice" : "n.nnnnnnnnnnn...",  // (string) the inverse unit price (sold/received)
    "sellertxid" : "hash",                // (string) the hash of the transaction of the seller
    "address" : "address",                // (string) the Bitcoin address of the seller
    "amountsold" : "n.nnnnnnnn",          // (string) the number of tokens sold in this trade
    "amountreceived" : "n.nnnnnnnn",      // (string) the number of tokens traded in exchange
    "matchingtxid" : "hash",              // (string) the hash of the transaction that was matched against
    "matchingaddress" : "address"         // (string) the Bitcoin address of the other party of this trade
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_gettradehistoryforpair" 1 12 500
```

---

### token_gettradehistoryforaddress

Retrieves the history of orders on the distributed exchange for the supplied address.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `address`           | string  | required | address to retrieve history for                                                              |
| `count`             | number  | optional | number of orders to retrieve (default: `10`)                                                 |
| `propertyid`        | number  | optional | filter by propertyid transacted (default: no filter)                                         |

**Result:**
```js
[                                             // (array of JSON objects)
  {
    "txid" : "hash",                              // (string) the hex-encoded hash of the transaction of the order
    "sendingaddress" : "address",                 // (string) the Bitcoin address of the trader
    "ismine" : true|false,                        // (boolean) whether the order involes an address in the wallet
    "confirmations" : nnnnnnnnnn,                 // (number) the number of transaction confirmations
    "fee" : "n.nnnnnnnn",                         // (string) the transaction fee in bitcoins
    "blocktime" : nnnnnnnnnn,                     // (number) the timestamp of the block that contains the transaction
    "valid" : true|false,                         // (boolean) whether the transaction is valid
    "positioninblock" : n,                        // (number) the position (index) of the transaction within the block
    "version" : n,                                // (number) the transaction version
    "type_int" : n,                               // (number) the transaction type as number
    "type" : "type",                              // (string) the transaction type as string
    "propertyidforsale" : n,                      // (number) the identifier of the tokens put up for sale
    "propertyidforsaleisdivisible" : true|false,  // (boolean) whether the tokens for sale are divisible
    "amountforsale" : "n.nnnnnnnn",               // (string) the amount of tokens initially offered
    "propertyiddesired" : n,                      // (number) the identifier of the tokens desired in exchange
    "propertyiddesiredisdivisible" : true|false,  // (boolean) whether the desired tokens are divisible
    "amountdesired" : "n.nnnnnnnn",               // (string) the amount of tokens initially desired
    "unitprice" : "n.nnnnnnnnnnn..."              // (string) the unit price (shown in the property desired)
    "status" : "status"                           // (string) the status of the order ("open", "cancelled", "filled", ...)
    "canceltxid" : "hash",                        // (string) the hash of the transaction that cancelled the order (if cancelled)
    "matches": [                                  // (array of JSON objects) a list of matched orders and executed trades
      {
        "txid" : "hash",                              // (string) the hash of the transaction that was matched against
        "block" : nnnnnn,                             // (number) the index of the block that contains this transaction
        "address" : "address",                        // (string) the Bitcoin address of the other trader
        "amountsold" : "n.nnnnnnnn",                  // (string) the number of tokens sold in this trade
        "amountreceived" : "n.nnnnnnnn"               // (string) the number of tokens traded in exchange
      },
      ...
    ]
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_gettradehistoryforaddress" "1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8"
```

---

### token_getactivations

Returns pending and completed feature activations.

**Result:**
```js
{
  "pendingactivations": [      // (array of JSON objects) a list of pending feature activations
    {
      "featureid" : n,             // (number) the id of the feature
      "featurename" : "xxxxxxxx",  // (string) the name of the feature
      "activationblock" : n,       // (number) the block the feature will be activated
      "minimumversion" : n         // (number) the minimum client version needed to support this feature
    },
    ...
  ]
  "completedactivations": [    // (array of JSON objects) a list of completed feature activations
    {
      "featureid" : n,             // (number) the id of the feature
      "featurename" : "xxxxxxxx",  // (string) the name of the feature
      "activationblock" : n,       // (number) the block the feature will be activated
      "minimumversion" : n         // (number) the minimum client version needed to support this feature
    },
    ...
  ]
}
```

**Example:**

```bash
$ tokencore-cli "token_getactivations"
```

---

### token_getpayload

Get the payload for an Token transaction.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `txid`              | string  | required | the hash of the transaction to retrieve payload                                              |

**Result:**
```js
{
  "payload" : "payloadmessage",  // (string) the decoded Token payload message
  "payloadsize" : n              // (number) the size of the payload
}
```

**Example:**

```bash
$ tokencore-cli "token_getactivations" "1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d"
```

---

### token_getseedblocks

Returns a list of blocks containing Token transactions for use in seed block filtering.

WARNING: The Exodus crowdsale is not stored in LevelDB, thus this is currently only safe to use to generate seed blocks after block 255365.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `startblock`        | number  | required | the first block to look for Token transactions (inclusive)                                    |
| `endblock`          | number  | required | the last block to look for Token transactions (inclusive)                                     |

**Result:**
```js
[         // (array of numbers) a list of seed blocks
  nnnnnnn,  // the block height of the seed block
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_getseedblocks" 290000 300000
```

---

### token_getcurrentconsensushash

Returns the consensus hash covering the state of the current block.

**Arguments:**

*None*

**Result:**
```js
{
  "block" : nnnnnn,         // (number) the index of the block this consensus hash applies to
  "blockhash" : "hash",     // (string) the hash of the corresponding block
  "consensushash" : "hash"  // (string) the consensus hash for the block
}
```

**Example:**

```bash
$ tokencore-cli "token_getcurrentconsensushash"
```

---

## Raw transactions

The RPCs for raw transactions/payloads can be used to decode or create raw Token transactions.

Raw transactions need to be signed with `"signrawtransaction"` and then broadcasted with `"sendrawtransaction"`.

### token_decodetransaction

Decodes an Token transaction.

If the inputs of the transaction are not in the chain, then they must be provided, because the transaction inputs are used to identify the sender of a transaction.

A block height can be provided, which is used to determine the parsing rules.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `rawtx`             | string  | required | the raw transaction to decode                                                                |
| `prevtxs`           | string  | optional | a JSON array of transaction inputs (default: none)                                           |
| `height`            | number  | optional | the parsing block height (default: 0 for chain height)                                       |

The format of `prevtxs` is as following:

```js
[
  {
    "txid" : "hash",         // (string, required) the transaction hash
    "vout" : n,              // (number, required) the output number
    "scriptPubKey" : "hex",  // (string, required) the output script
    "value" : n.nnnnnnnn     // (number, required) the output value
  }
  ,...
]
```

**Result:**
```js
{
  "txid" : "hash",                 // (string) the hex-encoded hash of the transaction
  "fee" : "n.nnnnnnnn",            // (string) the transaction fee in bitcoins
  "sendingaddress" : "address",    // (string) the Bitcoin address of the sender
  "referenceaddress" : "address",  // (string) a Bitcoin address used as reference (if any)
  "ismine" : true|false,           // (boolean) whether the transaction involes an address in the wallet
  "version" : n,                   // (number) the transaction version
  "type_int" : n,                  // (number) the transaction type as number
  "type" : "type",                 // (string) the transaction type as string
  [...]                            // (mixed) other transaction type specific properties
}
```

**Example:**

```bash
$ tokencore-cli "token_decodetransaction" "010000000163af14ce6d477e1c793507e32a5b7696288fa89705c0d02a3f66beb3c \
    5b8afee0100000000ffffffff02ac020000000000004751210261ea979f6a06f9dafe00fb1263ea0aca959875a7073556a088cdf \
    adcd494b3752102a3fd0a8a067e06941e066f78d930bfc47746f097fcd3f7ab27db8ddf37168b6b52ae22020000000000001976a \
    914946cb2e08075bcbaf157e47bcb67eb2b2339d24288ac00000000" \
    "[{\"txid\":\"eeafb8c5b3be663f2ad0c00597a88f2896765b2ae30735791c7e476dce14af63\",\"vout\":1, \
    \"scriptPubKey\":\"76a9149084c0bd89289bc025d0264f7f23148fb683d56c88ac\",\"value\":0.0001123}]"
```

---

### token_createrawtx_opreturn

Adds a payload with class C (op-return) encoding to the transaction.

If no raw transaction is provided, a new transaction is created.

If the data encoding fails, then the transaction is not modified.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `rawtx`             | string  | required | the raw transaction to extend (can be `null`)                                                |
| `payload`           | string  | required | the hex-encoded payload to add                                                               |

**Result:**
```js
"rawtx"  // (string) the hex-encoded modified raw transaction
```

**Example:**

```bash
$ tokencore-cli "token_createrawtx_opreturn" "01000000000000000000" "00000000000000020000000006dac2c0"
```

---

### token_createrawtx_multisig

Adds a payload with class B (bare-multisig) encoding to the transaction.

If no raw transaction is provided, a new transaction is created.

If the data encoding fails, then the transaction is not modified.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `rawtx`             | string  | required | the raw transaction to extend (can be `null`)                                                |
| `payload`           | string  | required | the hex-encoded payload to add                                                               |
| `seed`              | string  | required | the seed for obfuscation                                                                     |
| `payload`           | string  | required | a public key or address for dust redemption                                                  |

**Result:**
```js
"rawtx"  // (string) the hex-encoded modified raw transaction
```

**Example:**

```bash
$ tokencore-cli "token_createrawtx_multisig" \
    "0100000001a7a9402ecd77f3c9f745793c9ec805bfa2e14b89877581c734c774864247e6f50400000000ffffffff01aa0a00000 \
    00000001976a9146d18edfe073d53f84dd491dae1379f8fb0dfe5d488ac00000000" \
    "00000000000000020000000000989680"
    "1LifmeXYHeUe2qdKWBGVwfbUCMMrwYtoMm" \
    "0252ce4bdd3ce38b4ebbc5a6e1343608230da508ff12d23d85b58c964204c4cef3"
```

---

### token_createrawtx_input

Adds a transaction input to the transaction.

If no raw transaction is provided, a new transaction is created.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `rawtx`             | string  | required | the raw transaction to extend (can be `null`)                                                |
| `txid`              | string  | required | the hash of the input transaction                                                            |
| `n`                 | number  | required | the index of the transaction output used as input                                            |

**Result:**
```js
"rawtx"  // (string) the hex-encoded modified raw transaction
```

**Example:**

```bash
$ tokencore-cli "token_createrawtx_input" \
    "01000000000000000000" "b006729017df05eda586df9ad3f8ccfee5be340aadf88155b784d1fc0e8342ee" 0
```

---

### token_createrawtx_reference

Adds a reference output to the transaction.

If no raw transaction is provided, a new transaction is created.

The output value is set to at least the dust threshold.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `rawtx`             | string  | required | the raw transaction to extend (can be `null`)                                                |
| `destination`       | string  | required | the reference address or destination                                                         |
| `amount`            | number  | optional | the optional reference amount (minimal by default)                                           |

**Result:**
```js
"rawtx"  // (string) the hex-encoded modified raw transaction
```

**Example:**

```bash
$ tokencore-cli "token_createrawtx_reference" \
    "0100000001a7a9402ecd77f3c9f745793c9ec805bfa2e14b89877581c734c774864247e6f50400000000ffffffff03aa0a00000
    00000001976a9146d18edfe073d53f84dd491dae1379f8fb0dfe5d488ac5c0d0000000000004751210252ce4bdd3ce38b4ebbc5a
    6e1343608230da508ff12d23d85b58c964204c4cef3210294cc195fc096f87d0f813a337ae7e5f961b1c8a18f1f8604a909b3a51
    21f065b52aeaa0a0000000000001976a914946cb2e08075bcbaf157e47bcb67eb2b2339d24288ac00000000" \
    "1CE8bBr1dYZRMnpmyYsFEoexa1YoPz2mfB" \
    0.005
```

---

### token_createrawtx_change

Adds a change output to the transaction.

The provided inputs are not added to the transaction, but only used to determine the change. It is assumed that the inputs were previously added, for example via `"createrawtransaction"`.

Optionally a position can be provided, where the change output should be inserted, starting with `0`. If the number of outputs is smaller than the position, then the change output is added to the end. Change outputs should be inserted before reference outputs, and as per default, the change output is added to the`first position.

If the change amount would be considered as dust, then no change output is added.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `rawtx`             | string  | required | the raw transaction to extend                                                                |
| `prevtxs`           | string  | required | a JSON array of transaction inputs                                                           |
| `destination`       | string  | required | the destination for the change                                                               |
| `fee`               | number  | required | the desired transaction fees                                                                 |
| `position`          | number  | optional | the position of the change output (default: first position)                                  |

The format of `prevtxs` is as following:

```js
[
  {
    "txid" : "hash",         // (string, required) the transaction hash
    "vout" : n,              // (number, required) the output number
    "scriptPubKey" : "hex",  // (string, required) the output script
    "value" : n.nnnnnnnn     // (number, required) the output value
  }
  ,...
]
```

**Result:**
```js
"rawtx"  // (string) the hex-encoded modified raw transaction
```

**Example:**

```bash
$ tokencore-cli "token_createrawtx_change" \
    "0100000001b15ee60431ef57ec682790dec5a3c0d83a0c360633ea8308fbf6d5fc10a779670400000000ffffffff025c0d00000 \
    000000047512102f3e471222bb57a7d416c82bf81c627bfcd2bdc47f36e763ae69935bba4601ece21021580b888ff56feb27f17f \
    08802ebed26258c23697d6a462d43fc13b565fda2dd52aeaa0a0000000000001976a914946cb2e08075bcbaf157e47bcb67eb2b2 \
    339d24288ac00000000" \
    "[{\"txid\":\"6779a710fcd5f6fb0883ea3306360c3ad8c0a3c5de902768ec57ef3104e65eb1\",\"vout\":4, \
    \"scriptPubKey\":\"76a9147b25205fd98d462880a3e5b0541235831ae959e588ac\",\"value\":0.00068257}]" \
    "1CE8bBr1dYZRMnpmyYsFEoexa1YoPz2mfB" 0.000035 1
```

---

### token_createpayload_simplesend

Create the payload for a simple send transaction.

Note: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the tokens to send                                                         |
| `amount`            | string  | required | the amount to send                                                                           |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_simplesend" 1 "100.0"
```

---

### token_createpayload_sendall

Create the payload for a send all transaction.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `ecosystem`         | number  | required | the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)             |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_sendall" 2
```

---

### token_createpayload_dexsell

Create a payload to place, update or cancel a sell offer on the traditional distributed TOKEN/BTC exchange.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyidforsale` | number  | required | the identifier of the tokens to list for sale (must be 1 for OMN or 2 for TOMN)              |
| `amountforsale`     | string  | required | the amount of tokens to list for sale                                                        |
| `amountdesired`     | string  | required | the amount of bitcoins desired                                                               |
| `paymentwindow`     | number  | required | a time limit in blocks a buyer has to pay following a successful accepting order             |
| `minacceptfee`      | string  | required | a minimum mining fee a buyer has to pay to accept the offer                                  |
| `action`            | number  | required | the action to take (1 for new offers, 2 to update\", 3 to cancel)                            |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_dexsell" 1 "1.5" "0.75" 25 "0.0005" 1
```

---

### token_createpayload_dexaccept

Create the payload for an accept offer for the specified token and amount.

Note: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the token to purchase                                                      |
| `amount`            | string  | required | the amount to accept                                                                         |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_dexaccept" 1 "15.0"
```

---

### token_createpayload_sto

Creates the payload for a send-to-owners transaction.

Note: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!

**Arguments:**

| Name                   | Type    | Presence | Description                                                                                  |
|------------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`           | number  | required | the identifier of the token to distribute                                                    |
| `amount`               | string  | required | the amount to distribute                                                                     |
| `distributionproperty` | number  | optional | the identifier of the property holders to distribute to                                      |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_sto" 3 "5000"
```

---

### token_createpayload_issuancefixed

Creates the payload for a new tokens issuance with fixed supply.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `ecosystem`         | number  | required | the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)           |
| `type`              | number  | required | the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)         |
| `previousid`        | number  | required | an identifier of a predecessor token (use 0 for new tokens)                                  |
| `category`          | string  | required | a category for the new tokens (can be "")                                                    |
| `subcategory`       | string  | required | a subcategory for the new tokens (can be "")                                                 |
| `name`              | string  | required | the name of the new tokens to create                                                         |
| `url`               | string  | required | an URL for further information about the new tokens (can be "")                              |
| `data`              | string  | required | a description for the new tokens (can be "")                                                 |
| `amount`            | string  | required | the number of tokens to create                                                               |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_issuancefixed" 2 1 0 "Companies" "Bitcoin Mining" "Quantum Miner" "" "" "1000000"
```

---

### token_createpayload_issuancecrowdsale

Creates the payload for a new tokens issuance with crowdsale.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `ecosystem`         | number  | required | the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)           |
| `type`              | number  | required | the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)         |
| `previousid`        | number  | required | an identifier of a predecessor token (use 0 for new tokens)                                  |
| `category`          | string  | required | a category for the new tokens (can be "")                                                    |
| `subcategory`       | string  | required | a subcategory for the new tokens (can be "")                                                 |
| `name`              | string  | required | the name of the new tokens to create                                                         |
| `url`               | string  | required | an URL for further information about the new tokens (can be "")                              |
| `data`              | string  | required | a description for the new tokens (can be "")                                                 |
| `propertyiddesired` | number  | required | the identifier of a token eligible to participate in the crowdsale                           |
| `tokensperunit`     | string  | required | the amount of tokens granted per unit invested in the crowdsale                              |
| `deadline`          | number  | required | the deadline of the crowdsale as Unix timestamp                                              |
| `earlybonus`        | number  | required | an early bird bonus for participants in percent per week                                     |
| `issuerpercentage`  | number  | required | a percentage of tokens that will be granted to the issuer                                    |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_issuancecrowdsale" 2 1 0 "Companies" "Bitcoin Mining" "Quantum Miner" "" "" 2 "100" 1483228800 30 2
```

---

### token_createpayload_issuancemanaged

Creates the payload for a new tokens issuance with manageable supply.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `ecosystem`         | number  | required | the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)           |
| `type`              | number  | required | the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)         |
| `previousid`        | number  | required | an identifier of a predecessor token (use 0 for new tokens)                                  |
| `category`          | string  | required | a category for the new tokens (can be "")                                                    |
| `subcategory`       | string  | required | a subcategory for the new tokens (can be "")                                                 |
| `name`              | string  | required | the name of the new tokens to create                                                         |
| `url`               | string  | required | an URL for further information about the new tokens (can be "")                              |
| `data`              | string  | required | a description for the new tokens (can be "")                                                 |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_issuancemanaged" 2 1 0 "Companies" "Bitcoin Mining" "Quantum Miner" "" ""
```

---

### token_createpayload_closecrowdsale

Creates the payload to manually close a crowdsale.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the crowdsale to close                                                     |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_closecrowdsale" 70
```

---

### token_createpayload_grant

Creates the payload to issue or grant new units of managed tokens.

Note: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the tokens to grant                                                        |
| `amount`            | string  | required | the amount of tokens to create                                                               |
| `memo`              | string  | optional | a text note attached to this transaction (none by default)                                   |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_grant" 51 "7000"
```

---

### token_createpayload_revoke

Creates the payload to revoke units of managed tokens.

Note: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!f

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the tokens to revoke                                                       |
| `amount`            | string  | required | the amount of tokens to revoke                                                               |
| `memo`              | string  | optional | a text note attached to this transaction (none by default)                                   |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_revoke" 51 "100"
```

---

### token_createpayload_changeissuer

Creates the payload to change the issuer on record of the given tokens.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the tokens                                                                 |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_changeissuer" 3
```

---

### token_createpayload_trade

Creates the payload to place a trade offer on the distributed token exchange.

Note: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyidforsale` | number  | required | the identifier of the tokens to list for sale                                                |
| `amountforsale`     | string  | required | the amount of tokens to list for sale                                                        |
| `propertyiddesired` | number  | required | the identifier of the tokens desired in exchange                                             |
| `amountdesired`     | string  | required | the amount of tokens desired in exchange                                                     |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_trade" 31 "250.0" 1 "10.0"
```

---

### token_createpayload_canceltradesbyprice

Creates the payload to cancel offers on the distributed token exchange with the specified price.

Note: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyidforsale` | number  | required | the identifier of the tokens to list for sale                                                |
| `amountforsale`     | string  | required | the amount of tokens to list for sale                                                        |
| `propertyiddesired` | number  | required | the identifier of the tokens desired in exchange                                             |
| `amountdesired`     | string  | required | the amount of tokens desired in exchange                                                     |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_canceltradesbyprice" 31 "100.0" 1 "5.0"
```

---

### token_createpayload_canceltradesbypair

Creates the payload to cancel all offers on the distributed token exchange with the given currency pair.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyidforsale` | number  | required | the identifier of the tokens to list for sale                                                |
| `propertyiddesired` | number  | required | the identifier of the tokens desired in exchange                                             |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_canceltradesbypair" 1 31
```

---

### token_createpayload_cancelalltrades

Creates the payload to cancel all offers on the distributed token exchange with the given currency pair.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `ecosystem`         | number  | required | the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)           |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_cancelalltrades" 1
```

---

### token_createpayload_enablefreezing

Creates the payload to enable address freezing for a centrally managed property.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the tokens                                                                 |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_enablefreezing" 3
```

---

### token_createpayload_disablefreezing

Creates the payload to disable address freezing for a centrally managed property.

IMPORTANT NOTE:  Disabling freezing for a property will UNFREEZE all frozen addresses for that property!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the tokens                                                                 |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_disablefreezing" 3
```

---

### token_createpayload_freeze

Creates the payload to freeze an address for a centrally managed token.

Note: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                                 |
|---------------------|---------|----------|-------------------------------------------------------------------------------------------------------------|
| `toaddress`         | string  | required | the address to freeze tokens for                                                                            |
| `propertyid`        | number  | required | the property to freeze tokens for (must be managed type and have freezing option enabled)                   |
| `amount`            | string  | required | the amount of tokens to freeze (note: this is unused - once frozen an address cannot send any transactions) |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_freeze" "3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs" 31 "100"
```

---

### token_createpayload_unfreeze

Creates the payload to unfreeze an address for a centrally managed token.

Note: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!

**Arguments:**

| Name                | Type    | Presence | Description                                                                                                 |
|---------------------|---------|----------|-------------------------------------------------------------------------------------------------------------|
| `toaddress`         | string  | required | the address to unfreeze tokens for                                                                          |
| `propertyid`        | number  | required | the property to unfreeze tokens for (must be managed type and have freezing option enabled)                 |
| `amount`            | string  | required | the amount of tokens to unfreeze (note: this is unused)                                                     |

**Result:**
```js
"payload"  // (string) the hex-encoded payload
```

**Example:**

```bash
$ tokencore-cli "token_createpayload_unfreeze" "3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs" 31 "100"
```

---

## Fee system

The RPCs for the fee system can be used to obtain data about the fee system and fee distributions.

### token_getfeecache

Obtains the current amount of fees cached (pending distribution).

If a property ID is supplied the results will be filtered to show this property ID only.  If no property ID is supplied the results will contain all properties that currently have fees cached pending distribution.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | optional | the identifier of the property to filter results on                                          |

**Result:**
```js
[                                  // (array of JSON objects)
  {
    "propertyid" : nnnnnnn,        // (number) the property id
    "cachedfees" : "n.nnnnnnnn",   // (string) the amount of fees cached for this property
  },
...
]
```

**Example:**

```bash
$ tokencore-cli "token_getfeecache" 31
```

---

### token_getfeetrigger

Obtains the amount at which cached fees will be distributed.

If a property ID is supplied the results will be filtered to show this property ID only.  If no property ID is supplied the results will contain all properties.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | optional | the identifier of the property to filter results on                                          |

**Result:**
```js
[                                  // (array of JSON objects)
  {
    "propertyid" : nnnnnnn,        // (number) the property id
    "feetrigger" : "n.nnnnnnnn",   // (string) the amount of fees required to trigger distribution
  },
...
]
```

**Example:**

```bash
$ tokencore-cli "token_getfeetrigger" 31
```

---

### token_getfeeshare

Obtains the current percentage share of fees addresses would receive if a distribution were to occur.

If an address is supplied the results will be filtered to show this address only.  If no address is supplied the results will be filtered to show wallet addresses only.  If a wildcard is provided (```"*"```) the results will contain all addresses that would receive a share.

If an ecosystem is supplied the results will reflect the fee share for that ecosystem (main or test).  If no ecosystem is supplied the results will reflect the main ecosystem.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `address`           | string  | optional | the address to filter results on                                                             |
| `ecosystem`         | number  | optional | the ecosystem to obtain the current percentage fee share (1 = main, 2 = test)                |

**Result:**
```js
[                                  // (array of JSON objects)
  {
    "address" : "address"          // (string) the adress that would receive a share of fees
    "feeshare" : "n.nnnn%",        // (string) the percentage of fees this address will receive based on the current state
  },
...
]
```

**Example:**

```bash
$ tokencore-cli "token_getfeeshare" "1CE8bBr1dYZRMnpmyYsFEoexa1YoPz2mfB" 1
```

---

### token_getfeedistribution

Obtains data for a past distribution of fees.

A distribution ID must be supplied to identify the distribution to obtain data for.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `distributionid`    | number  | required | the identifier of the distribution to obtain data for                                        |

**Result:**
```js
{
  "distributionid" : n,            // (number) the distribution id
  "propertyid" : n,                // (number) the property id of the distributed tokens
  "block" : n,                     // (number) the block the distribution occurred
  "amount" : "n.nnnnnnnn",         // (string) the amount that was distributed
  "recipients": [                  // (array of JSON objects) a list of recipients
    {
      "address" : "address",       // (string) the address of the recipient
      "amount" : "n.nnnnnnnn"      // (string) the amount of fees received by the recipient
    },
    ...
  ]
}
```

**Example:**

```bash
$ tokencore-cli "token_getfeedistribution" 1
```

---

### token_getfeedistributions

Obtains data for past distributions of fees for a property.

A property ID must be supplied to retrieve past distributions for.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `propertyid`        | number  | required | the identifier of the property to retrieve past distributions for                            |

**Result:**
```js
[                                  // (array of JSON objects)
  {
    "distributionid" : n,          // (number) the distribution id
    "propertyid" : n,              // (number) the property id of the distributed tokens
    "block" : n,                   // (number) the block the distribution occurred
    "amount" : "n.nnnnnnnn",       // (string) the amount that was distributed
    "recipients": [                // (array of JSON objects) a list of recipients
      {
        "address" : "address",       // (string) the address of the recipient
        "amount" : "n.nnnnnnnn"      // (string) the amount of fees received by the recipient
      },
      ...
    ]
  },
  ...
]
```

**Example:**

```bash
$ tokencore-cli "token_getfeedistributions" 31
```

---

## Configuration

The RPCs for the configuration can be used to alter Token Core settings.

### token_setautocommit

Sets the global flag that determines whether transactions are automatically committed and broadcasted.

**Arguments:**

| Name                | Type    | Presence | Description                                                                                  |
|---------------------|---------|----------|----------------------------------------------------------------------------------------------|
| `flag`              | boolean | required | the flag                                                                                     |

**Result:**
```js
true|false  // (boolean) the updated flag status
```

**Example:**

```bash
$ tokencore-cli "token_setautocommit" false
```

---

## Depreciated API calls

To ensure backwards compatibility, depreciated RPCs are kept for at least one major version.

The following calls are replaced in Token Core 0.0.10, and queries with the old command are forwarded.

- `send_MP` by `token_send`
- `sendtoowners_MP` by `token_sendsto`
- `sendrawtx_MP` by `token_sendrawtx`
- `getinfo_MP` by `token_getinfo`
- `getbalance_MP` by `token_getbalance`
- `getallbalancesforid_MP` by `token_getallbalancesforid`
- `getallbalancesforaddress_MP` by `token_getallbalancesforaddress`
- `gettransaction_MP` by `token_gettransaction`
- `listtransactions_MP` by `token_listtransactions`
- `listblocktransactions_MP` by `token_listblocktransactions`
- `getactivedexsells_MP` by `token_getactivedexsells`
- `listproperties_MP` by `token_listproperties`
- `getproperty_MP` by `token_getproperty`
- `getactivecrowdsales_MP` by `token_getactivecrowdsales`
- `getcrowdsale_MP` by `token_getcrowdsale`
- `getgrants_MP` by `token_getgrants`
- `getsto_MP` by `token_getsto` or `token_gettransaction`
