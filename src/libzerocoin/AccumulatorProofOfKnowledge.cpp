/**
 * @file       AccumulatorProofOfKnowledge.cpp
 *
 * @brief      AccumulatorProofOfKnowledge class for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017-2019 The PIVX developers

#include "AccumulatorProofOfKnowledge.h"
#include "hash.h"

namespace libzerocoin {

AccumulatorProofOfKnowledge::AccumulatorProofOfKnowledge(const AccumulatorAndProofParams* p): params(p) {}

/** Verifies that a commitment c is accumulated in accumulator a
 */
bool AccumulatorProofOfKnowledge:: Verify(const Accumulator& a, const CBigNum& valueOfCommitmentToCoin) const {
    CBigNum sg = params->accumulatorPoKCommitmentGroup.g;
    CBigNum sh = params->accumulatorPoKCommitmentGroup.h;

    CBigNum g_n = params->accumulatorQRNCommitmentGroup.g;
    CBigNum h_n = params->accumulatorQRNCommitmentGroup.h;

    //According to the proof, this hash should be of length k_prime bits.  It is currently greater than that, which should not be a problem, but we should check this.
    CHashWriter hasher(0,0);
    hasher << *params << sg << sh << g_n << h_n << valueOfCommitmentToCoin << C_e << C_u << C_r << st_1 << st_2 << st_3 << t_1 << t_2 << t_3 << t_4;

    CBigNum c = CBigNum(hasher.GetHash()); //this hash should be of length k_prime bits

    CBigNum st_1_prime = (valueOfCommitmentToCoin.pow_mod(c, params->accumulatorPoKCommitmentGroup.modulus) * sg.pow_mod(s_alpha, params->accumulatorPoKCommitmentGroup.modulus) * sh.pow_mod(s_phi, params->accumulatorPoKCommitmentGroup.modulus)) % params->accumulatorPoKCommitmentGroup.modulus;
    CBigNum st_2_prime = (sg.pow_mod(c, params->accumulatorPoKCommitmentGroup.modulus) * ((valueOfCommitmentToCoin * sg.inverse(params->accumulatorPoKCommitmentGroup.modulus)).pow_mod(s_gamma, params->accumulatorPoKCommitmentGroup.modulus)) * sh.pow_mod(s_psi, params->accumulatorPoKCommitmentGroup.modulus)) % params->accumulatorPoKCommitmentGroup.modulus;
    CBigNum st_3_prime = (sg.pow_mod(c, params->accumulatorPoKCommitmentGroup.modulus) * (sg * valueOfCommitmentToCoin).pow_mod(s_sigma, params->accumulatorPoKCommitmentGroup.modulus) * sh.pow_mod(s_xi, params->accumulatorPoKCommitmentGroup.modulus)) % params->accumulatorPoKCommitmentGroup.modulus;

    CBigNum t_1_prime = (C_r.pow_mod(c, params->accumulatorModulus) * h_n.pow_mod(s_zeta, params->accumulatorModulus) * g_n.pow_mod(s_epsilon, params->accumulatorModulus)) % params->accumulatorModulus;
    CBigNum t_2_prime = (C_e.pow_mod(c, params->accumulatorModulus) * h_n.pow_mod(s_eta, params->accumulatorModulus) * g_n.pow_mod(s_alpha, params->accumulatorModulus)) % params->accumulatorModulus;
    CBigNum t_3_prime = ((a.getValue()).pow_mod(c, params->accumulatorModulus) * C_u.pow_mod(s_alpha, params->accumulatorModulus) * ((h_n.inverse(params->accumulatorModulus)).pow_mod(s_beta, params->accumulatorModulus))) % params->accumulatorModulus;
    CBigNum t_4_prime = (C_r.pow_mod(s_alpha, params->accumulatorModulus) * ((h_n.inverse(params->accumulatorModulus)).pow_mod(s_delta, params->accumulatorModulus)) * ((g_n.inverse(params->accumulatorModulus)).pow_mod(s_beta, params->accumulatorModulus))) % params->accumulatorModulus;

    bool result_st1 = (st_1 == st_1_prime);
    bool result_st2 = (st_2 == st_2_prime);
    bool result_st3 = (st_3 == st_3_prime);

    bool result_t1 = (t_1 == t_1_prime);
    bool result_t2 = (t_2 == t_2_prime);
    bool result_t3 = (t_3 == t_3_prime);
    bool result_t4 = (t_4 == t_4_prime);

    bool result_range = ((s_alpha >= -(params->maxCoinValue * BN_TWO.pow(params->k_prime + params->k_dprime + 1))) && (s_alpha <= (params->maxCoinValue * BN_TWO.pow(params->k_prime + params->k_dprime + 1))));

    return result_st1 && result_st2 && result_st3 && result_t1 && result_t2 && result_t3 && result_t4 && result_range;
}

} /* namespace libzerocoin */
