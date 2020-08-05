// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"

#include <array>
#include <stdexcept>

#include "sapling/note.hpp"
#include "sapling/noteencryption.hpp"
#include "sapling/prf.h"
#include "sapling/address.hpp"
#include "sapling/util.h"
#include "crypto/sha256.h"

#include <boost/test/unit_test.hpp>
#include <librustzcash.h>
#include <sodium.h>

BOOST_FIXTURE_TEST_SUITE(noteencryption_tests, BasicTestingSetup)

class TestNoteDecryption : public ZCNoteDecryption {
public:
    TestNoteDecryption(uint256 sk_enc) : ZCNoteDecryption(sk_enc) {}

    void change_pk_enc(uint256 to) {
        pk_enc = to;
    }
};

BOOST_AUTO_TEST_CASE(note_plain_text)
{
    using namespace libzcash;
    auto xsk = SaplingSpendingKey(uint256()).expanded_spending_key();
    auto fvk = xsk.full_viewing_key();
    auto ivk = fvk.in_viewing_key();
    SaplingPaymentAddress addr = *ivk.address({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});

    std::array<unsigned char, ZC_MEMO_SIZE> memo;
    for (size_t i = 0; i < ZC_MEMO_SIZE; i++) {
        // Fill the message with dummy data
        memo[i] = (unsigned char) i;
    }

    SaplingNote note(addr, 39393);
    auto cmu_opt = note.cm();
    if (!cmu_opt) {
        BOOST_ERROR("SaplingNote cm failed");
    }
    uint256 cmu = cmu_opt.get();
    SaplingNotePlaintext pt(note, memo);

    auto res = pt.encrypt(addr.pk_d);
    if (!res) {
        BOOST_ERROR("SaplingNotePlaintext encrypt failed");
    }

    auto enc = res.get();

    auto ct = enc.first;
    auto encryptor = enc.second;
    auto epk = encryptor.get_epk();

    // Try to decrypt with incorrect commitment
    BOOST_CHECK(!SaplingNotePlaintext::decrypt(
        ct,
        ivk,
        epk,
        uint256()
    ));

    // Try to decrypt with correct commitment
    auto foo = SaplingNotePlaintext::decrypt(
        ct,
        ivk,
        epk,
        cmu
    );

    if (!foo) {
        BOOST_ERROR("SaplingNotePlaintext decrypt failed");
    }

    auto bar = foo.get();

    BOOST_CHECK(bar.value() == pt.value());
    BOOST_CHECK(bar.memo() == pt.memo());
    BOOST_CHECK(bar.d == pt.d);
    BOOST_CHECK(bar.rcm == pt.rcm);

    auto foobar = bar.note(ivk);

    if (!foobar) {
        // Improve this message
        BOOST_ERROR("Invalid note");
    }

    auto new_note = foobar.get();

    BOOST_CHECK(note.value() == new_note.value());
    BOOST_CHECK(note.d == new_note.d);
    BOOST_CHECK(note.pk_d == new_note.pk_d);
    BOOST_CHECK(note.r == new_note.r);
    BOOST_CHECK(note.cm() == new_note.cm());

    SaplingOutgoingPlaintext out_pt;
    out_pt.pk_d = note.pk_d;
    out_pt.esk = encryptor.get_esk();

    auto ovk = random_uint256();
    auto cv = random_uint256();
    auto cm = random_uint256();

    auto out_ct = out_pt.encrypt(
        ovk,
        cv,
        cm,
        encryptor
    );

    auto decrypted_out_ct = out_pt.decrypt(
        out_ct,
        ovk,
        cv,
        cm,
        encryptor.get_epk()
    );

    if (!decrypted_out_ct) {
        BOOST_ERROR("SaplingOutgoingPlaintext decrypt failed");
    }

    auto decrypted_out_ct_unwrapped = decrypted_out_ct.get();

    BOOST_CHECK(decrypted_out_ct_unwrapped.pk_d == out_pt.pk_d);
    BOOST_CHECK(decrypted_out_ct_unwrapped.esk == out_pt.esk);

    // Test sender won't accept invalid commitments
    BOOST_CHECK(!SaplingNotePlaintext::decrypt(
        ct,
        epk,
        decrypted_out_ct_unwrapped.esk,
        decrypted_out_ct_unwrapped.pk_d,
        uint256()
    ));

    // Test sender can decrypt the note ciphertext.
    foo = SaplingNotePlaintext::decrypt(
        ct,
        epk,
        decrypted_out_ct_unwrapped.esk,
        decrypted_out_ct_unwrapped.pk_d,
        cmu
    );

    if (!foo) {
        BOOST_ERROR("Sender decrypt note ciphertext failed.");
    }

    bar = foo.get();

    BOOST_CHECK(bar.value() == pt.value());
    BOOST_CHECK(bar.memo() == pt.memo());
    BOOST_CHECK(bar.d == pt.d);
    BOOST_CHECK(bar.rcm == pt.rcm);
}

BOOST_AUTO_TEST_SUITE_END()
