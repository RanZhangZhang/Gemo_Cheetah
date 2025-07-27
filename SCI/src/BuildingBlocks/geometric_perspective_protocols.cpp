// anonymous authors
#include "BuildingBlocks/aux-protocols.h"
#include "geometric_perspective_protocols.h"

using namespace sci;

GeometricPerspectiveProtocols::GeometricPerspectiveProtocols(int party, sci::NetIO *io, sci::OTPack<sci::NetIO> *otpack) {
    this->party = party;
    this->io = io;
    this->otpack = otpack;
    this->aux = new AuxProtocols(party, io, otpack);
    this->mill = this->aux->mill;
    this->mill_eq = new MillionaireWithEquality(party, io, otpack);
    this->eq = new Equality(party, io, otpack);
    this->triple_gen = this->mill->triple_gen;
}

GeometricPerspectiveProtocols::~GeometricPerspectiveProtocols() {
    delete this->aux;
    delete this->mill_eq;
    delete this->eq;
}

void GeometricPerspectiveProtocols::new_truncate(int32_t dim, uint64_t *inA, uint64_t *outB,
                          int32_t shift, int32_t bw) {
    if (shift == 0) {
        memcpy(outB, inA, sizeof(uint64_t) * dim);
        return;
    }
    assert((bw - shift) > 0 && "Truncation shouldn't truncate the full bitwidth");
    assert(bw - shift - 1 >= 0);
    assert(inA != outB);

    uint64_t mask_bw = (bw == 64 ? -1 : ((1ULL << bw) - 1));
    uint64_t mask_shift = (shift == 64 ? -1 : ((1ULL << shift) - 1));
    uint64_t mask_upper =
            ((bw - shift) == 64 ? -1 : ((1ULL << (bw - shift)) - 1));

    uint64_t *inA_orig = new uint64_t[dim];
    uint64_t *inA_lower = new uint64_t[dim];
    uint64_t *inA_upper = new uint64_t[dim];
    uint8_t *wrap_lower = new uint8_t[dim];
    uint8_t *wrap_upper = new uint8_t[dim];
    uint8_t *eq_upper = new uint8_t[dim];
    uint8_t *and_upper = new uint8_t[dim];
    for (int i = 0; i < dim; i++) {
        inA_lower[i] = inA[i] & mask_shift;
        inA_upper[i] = (inA[i] >> shift) & mask_upper;
        if (party == sci::BOB) {
            inA_upper[i] = (mask_upper - inA_upper[i]) & mask_upper;
        }
    }
    uint64_t *inA_prime = new uint64_t[dim];
    uint64_t quarter = 1ULL << (bw - 2);
    if (party == sci::ALICE) {
        for (int i = 0; i < dim; i++) {
            inA_prime[i] = (inA[i] - quarter) & mask_bw;
        }
    } else {
        for (int i = 0; i < dim; i++) {
            inA_prime[i] = inA[i];
        }
    }
    uint64_t *r = new uint64_t[dim];
    uint64_t half = 1ULL << (bw - 1);
    for (int i = 0; i < dim; i++) {
        if (inA_prime[i] >= half) {
            r[i] = 1;
        } else {
            r[i] = 0;
        }
    }
    uint64_t *bit_mul = new uint64_t[dim];
    if (party == sci::ALICE) {
        sci::PRG128 prg;
        uint64_t *data0 = new uint64_t[dim];
        prg.random_data(data0, dim * sizeof(uint64_t));
        otpack->iknp_straight->send_cot(data0, r, dim, shift);
        for (int i = 0; i < dim; i++) {
            bit_mul[i] = ((1ULL << shift) - data0[i]) & mask_shift;
        }
        delete[] data0;
    } else { // party == BOB
        bool *choice = new bool[dim];
        for (int i = 0; i < dim; i++) {
            choice[i] = r[i];
        }
        uint64_t *data = new uint64_t[dim];
        otpack->iknp_straight->recv_cot(data, choice, dim, shift);
        for (int i = 0; i < dim; i++) {
            bit_mul[i] = data[i];
        }
        delete[] choice;
    }
    uint64_t *arith_wrap_lower = new uint64_t[dim];
    for (int i = 0; i < dim; i++) {
      arith_wrap_lower[i] = 0;
    }
    this->aux->wrap_computation(inA_lower, wrap_lower, dim, shift);
    this->aux->B2A(wrap_lower, arith_wrap_lower, dim, bw);
    uint64_t offset = 1ULL << (bw - shift - 2);
    for (int i = 0; i < dim; i++) {
        if (party == sci::ALICE) {
            outB[i] = (((inA_prime[i] >> shift) & mask_upper) + offset + arith_wrap_lower[i] -
                       (1ULL << (bw - shift)) * (bit_mul[i] + 1)) &
                      mask_bw;
        } else {
            outB[i] = (((inA_prime[i] >> shift) & mask_upper) + arith_wrap_lower[i] -
                       (1ULL << (bw - shift)) * bit_mul[i]) &
                      mask_bw;
        }
    }
    delete[] inA_orig;
    delete[] inA_lower;
    delete[] inA_upper;
    delete[] wrap_lower;
    delete[] wrap_upper;
    delete[] eq_upper;
    delete[] and_upper;
    delete[] arith_wrap_lower;

    return;
}

void GeometricPerspectiveProtocols::mw(int32_t dim, uint64_t *input, uint64_t *output, int32_t in_bw, int32_t out_bw) {
    uint64_t mask_in = (in_bw == 64 ? -1 : ((1ULL << in_bw) - 1));
    uint64_t mask_out = (out_bw == 64 ? -1 : ((1ULL << out_bw) - 1));
    uint64_t *input_prime = new uint64_t[dim];
    uint64_t quarter = 1ULL << (in_bw - 2);
    if (party == sci::ALICE) {
        for (int i = 0; i < dim; i++) {
            input_prime[i] = (input[i] - quarter) & mask_in;
        }
    } else {
        for (int i = 0; i < dim; i++) {
            input_prime[i] = input[i];
        }
    }
    uint64_t *r = new uint64_t[dim];
    uint64_t half = 1ULL << (in_bw - 1);
    for (int i = 0; i < dim; i++) {
        if (input_prime[i] >= half) {
            r[i] = 1;
        } else {
            r[i] = 0;
        }
    }
    uint64_t *mul = new uint64_t[dim];
    bit_mul(dim, r, mul, out_bw);
    for (int i = 0; i < dim; i++) {
        if (party == sci::ALICE) {
            output[i] = mul[i] + 1;
            if (input[i] < quarter) {
                output[i] = (output[i] - 1) & mask_out;
            }
        } else {
            output[i] = mul[i];
        }
    }
}

void GeometricPerspectiveProtocols::mux_3(int32_t dim, uint64_t *inA, uint64_t *inC, uint64_t *out, int32_t bw) {
    uint64_t mask_bw = (bw == 64 ? -1 : ((1ULL << bw) - 1));
    uint8_t *c0 = new uint8_t[dim];
    uint8_t *c1 = new uint8_t[dim];
    for (int i = 0; i < dim; i++) {
        bool *temp = new bool[2];
        sci::int64_to_bool(temp, inC[i], 2);
        c1[i] = temp[0];
        c0[i] = temp[1];
        delete[] temp;
    }
    uint8_t *carry = new uint8_t[dim];
    if (party == sci::ALICE) {
        uint8_t *dummy = new uint8_t[dim];
        for (int i = 0; i < dim; i++) {
            dummy[i] = 0;
        }
        aux->AND(c1, dummy, carry, dim);
    } else {
        uint8_t *dummy = new uint8_t[dim];
        for (int i = 0; i < dim; i++) {
            dummy[i] = 0;
        }
        aux->AND(dummy, c1, carry, dim);
    }
    uint64_t *temp_inA = new uint64_t[dim];
    for (int i = 0; i < dim; i++) {
        c0[i] = c0[i] ^ carry[i];
        temp_inA[i] = (inA[i] * 2) & mask_bw;
    }
    uint64_t *t1 = new uint64_t[dim];
    uint64_t *t2 = new uint64_t[dim];
    aux->multiplexer(c1, inA, t1, dim, bw, bw);
    aux->multiplexer(c0, temp_inA, t2, dim, bw, bw);
    for (int i = 0; i < dim; i++) {
        out[i] = (t1[i] + t2[i]) & mask_bw;
    }
}

void GeometricPerspectiveProtocols::bit_mul(int32_t dim, uint64_t *input, uint64_t *output, int32_t output_bw) {
    uint64_t mask_out = (output_bw == 64 ? -1 : ((1ULL << output_bw) - 1));
    if (party == sci::ALICE) {
        sci::PRG128 prg;
        uint64_t *data0 = new uint64_t[dim];
        prg.random_data(data0, dim * sizeof(uint64_t));
        otpack->iknp_straight->send_cot(data0, input, dim, output_bw);
        for (int i = 0; i < dim; i++) {
            output[i] = ((1ULL << output_bw) - data0[i]) & mask_out;
        }
        delete[] data0;
    } else { // party == BOB
        bool *choice = new bool[dim];
        for (int i = 0; i < dim; i++) {
            choice[i] = input[i];
        }
        uint64_t *data = new uint64_t[dim];
        otpack->iknp_straight->recv_cot(data, choice, dim, output_bw);
        for (int i = 0; i < dim; i++) {
            output[i] = data[i];
        }
        delete[] choice;
    }
}

void GeometricPerspectiveProtocols::cross_term(int32_t dim, uint64_t *inA, uint64_t *inB, uint64_t *outC,
                                               int32_t bwA, int32_t bwB, int32_t bwC) {
    uint64_t mask = (1ULL << bwC) - 1;
    if (party == sci::ALICE) {
        sci::PRG128 prg;
        for (int i = 0; i < bwB; i++) {
            auto *data0 = new uint64_t[dim];
            prg.random_data(data0, dim * sizeof(uint64_t));
            for (int j = 0; j < dim; j++) {
                data0[j] = data0[j] & ((1ULL << (bwC - i)) - 1);
            }
            otpack->iknp_straight->send_cot(data0, inA, dim, bwC - i);
            for (int j = 0; j < dim; j++) {
                outC[j] += (-data0[j] * (1ULL << i));
                outC[j] &= mask;
            }
            delete[] data0;
        }
    } else {
        bool choice[bwB][dim];
        for (int i = 0; i < dim; i++) {
            bool *temp = new bool[bwB];
            sci::int64_to_bool(temp, inB[i], bwB);
            for (int j = 0; j < bwB; j++) {
                choice[j][i] = temp[j];
            }
            delete[] temp;
        }
        for (int i = 0; i < bwB; i++) {
            auto *data = new uint64_t[dim];
            bool *c = new bool[dim];
            for (int j = 0; j < dim; j++) {
                bool *temp = new bool[bwB];
                sci::int64_to_bool(temp, inB[j], bwB);
                c[j] = temp[i];
            }
            otpack->iknp_straight->recv_cot(data, c, dim, bwC - i);
            for (int j = 0; j < dim; j++) {
                outC[j] += (data[j] * (1ULL << i));
                outC[j] &= mask;
            }
            delete[] data;
            delete[] c;
        }
    }
}

void GeometricPerspectiveProtocols::cross_term_reverse(int32_t dim, uint64_t *inA, uint64_t *inB, uint64_t *outC,
                                                       int32_t bwA, int32_t bwB, int32_t bwC) {
    uint64_t mask = (1ULL << bwC) - 1;
    if (party == sci::ALICE) {
        bool choice[bwA][dim];
        for (int i = 0; i < dim; i++) {
            bool *temp = new bool[bwA];
            sci::int64_to_bool(temp, inA[i], bwA);
            for (int j = 0; j < bwA; j++) {
                choice[j][i] = temp[j];
            }
            delete[] temp;
        }
        for (int i = 0; i < bwA; i++) {
            auto *data = new uint64_t[dim];
            bool *c = new bool[dim];
            for (int j = 0; j < dim; j++) {
                bool *temp = new bool[bwA];
                sci::int64_to_bool(temp, inA[j], bwA);
                c[j] = temp[i];
            }
            otpack->iknp_reversed->recv_cot(data, c, dim, bwC - i);
            for (int j = 0; j < dim; j++) {
                outC[j] += ((data[j] * (1ULL << i)) & mask);
                outC[j] &= mask;
            }
            delete[] data;
            delete[] c;
        }
    } else {
        sci::PRG128 prg;
        for (int i = 0; i < bwA; i++) {
            auto *data0 = new uint64_t[dim];
            prg.random_data(data0, dim * sizeof(uint64_t));
            otpack->iknp_reversed->send_cot(data0, inB, dim, bwC - i);
            for (int j = 0; j < dim; j++) {
                outC[j] += ((-data0[j] * (1ULL << i)) & mask);
                outC[j] &= mask;
            }
            delete[] data0;
        }
    }
}

void GeometricPerspectiveProtocols::signed_mul(int32_t dim, uint64_t *inA, uint64_t *inB, uint64_t *outC,
                                                    int32_t bwA, int32_t bwB, int32_t bwC) {
    auto *c = new uint64_t[dim];
    auto *d = new uint64_t[dim];
    for (int i = 0; i < dim; i++) {
        c[i] = 0;
        d[i] = 0;
    }
    if (bwA <= bwB) {
        cross_term_reverse(dim, inA, inB, c, bwA, bwB, bwC);
        cross_term(dim, inB, inA, d, bwB, bwA, bwC);
    } else {
        cross_term(dim, inA, inB, c, bwA, bwB, bwC);
        cross_term_reverse(dim, inB, inA, d, bwB, bwA, bwC);
    }
    auto *m_x = new uint64_t[dim];
    auto *m_y = new uint64_t[dim];
    mw(dim, inA, m_x, bwA, 2);
    mw(dim, inB, m_y, bwB, 2);
    auto *g = new uint64_t[dim];
    auto *h = new uint64_t[dim];
    mux_3(dim, inA, m_y, g, bwA);
    mux_3(dim, inB, m_x, h, bwB);
    uint64_t mask = (1ULL << bwC) - 1;
    for (int i = 0; i < dim; i++) {
        outC[i] = (inA[i] * inB[i] + c[i] + d[i] - (g[i] * (1ULL << bwB)) - (h[i] * (1ULL << bwA))) & mask;
    }
    delete[] c;
    delete[] d;
}

void GeometricPerspectiveProtocols::sign_extension(int32_t dim, uint64_t *in, uint64_t *out, int32_t in_bw, int32_t out_bw) {
    if (in_bw == out_bw) {
      memcpy(out, in, sizeof(uint64_t) * dim);
      return;
    }
    assert(in_bw < out_bw);
    uint64_t *c = new uint64_t[dim];
    for (int i = 0; i < dim; i++) {
        c[i] = 0;
    }
    mw(dim, in, c, in_bw, out_bw - in_bw);
    uint64_t M = 1ULL << in_bw;
    uint64_t N = 1ULL << out_bw;
    uint64_t mask = N - 1;
    for (int i = 0; i < dim; i++) {
        out[i] = (in[i] + c[i] * (N - M)) & mask;
    }
}

void GeometricPerspectiveProtocols::truncate_with_one_bit_error(int32_t dim, uint64_t *inA, uint64_t *outB,
                                                                int32_t shift, int32_t bw) {
    if (shift == 0) {
        memcpy(outB, inA, sizeof(uint64_t) * dim);
        return;
    }
    assert((bw - shift) > 0 && "Truncation shouldn't truncate the full bitwidth");
    assert(inA != outB);
    uint64_t mask_bw = (bw == 64 ? -1 : ((1ULL << bw) - 1));
    uint64_t mask_shift = (shift == 64 ? -1 : ((1ULL << shift) - 1));
    uint64_t mask_upper =
            ((bw - shift) == 64 ? -1 : ((1ULL << (bw - shift)) - 1));

    uint64_t *inA_orig = new uint64_t[dim];
    uint64_t *inA_lower = new uint64_t[dim];
    uint64_t *inA_upper = new uint64_t[dim];
    uint8_t *wrap_lower = new uint8_t[dim];
    uint8_t *wrap_upper = new uint8_t[dim];
    uint8_t *eq_upper = new uint8_t[dim];
    uint8_t *and_upper = new uint8_t[dim];
    for (int i = 0; i < dim; i++) {
        inA_lower[i] = inA[i] & mask_shift;
        inA_upper[i] = (inA[i] >> shift) & mask_upper;
        if (party == sci::BOB) {
            inA_upper[i] = (mask_upper - inA_upper[i]) & mask_upper;
        }
    }
    uint64_t *inA_prime = new uint64_t[dim];
    uint64_t quarter = 1ULL << (bw - 2);
    if (party == sci::ALICE) {
        // x - L/4
        for (int i = 0; i < dim; i++) {
            inA_prime[i] = (inA[i] - quarter) & mask_bw;
        }
    } else {
        for (int i = 0; i < dim; i++) {
            inA_prime[i] = inA[i];
        }
    }
    uint64_t *r = new uint64_t[dim];
    uint64_t half = 1ULL << (bw - 1);
    for (int i = 0; i < dim; i++) {
        if (inA_prime[i] >= half) {
            r[i] = 1;
        } else {
            r[i] = 0;
        }
    }
    uint64_t *bit_mul = new uint64_t[dim];
    if (party == sci::ALICE) {
        uint64_t mask_shift = (1ULL << shift) - 1;
        sci::PRG128 prg;
        uint64_t *data0 = new uint64_t[dim];
        prg.random_data(data0, dim * sizeof(uint64_t));
        otpack->iknp_straight->send_cot(data0, r, dim, shift);
        for (int i = 0; i < dim; i++) {
            bit_mul[i] = ((1ULL << shift) - data0[i]) & mask_shift;
        }
        delete[] data0;
    } else { // party == BOB
        bool *choice = new bool[dim];
        for (int i = 0; i < dim; i++) {
            choice[i] = r[i];
        }
        uint64_t *data = new uint64_t[dim];
        otpack->iknp_straight->recv_cot(data, choice, dim, shift);
        for (int i = 0; i < dim; i++) {
            bit_mul[i] = data[i];
        }
        delete[] choice;
    }
    uint64_t offset = 1ULL << (bw - shift - 2);
    for (int i = 0; i < dim; i++) {
        if (party == sci::ALICE) {
            outB[i] = (((inA_prime[i] >> shift) & mask_upper) + offset -
                       (1ULL << (bw - shift)) * (bit_mul[i] + 1)) &
                      mask_bw;
        } else {
            outB[i] = (((inA_prime[i] >> shift) & mask_upper) -
                       (1ULL << (bw - shift)) * bit_mul[i]) &
                      mask_bw;
        }
    }
    delete[] inA_orig;
    delete[] inA_lower;
    delete[] inA_upper;
    delete[] wrap_lower;
    delete[] wrap_upper;
    delete[] eq_upper;
    delete[] and_upper;
    return;
}

void GeometricPerspectiveProtocols::sirnn_unsigned_mul(int32_t dim, uint64_t *inA, uint64_t *inB, uint64_t *outC,
                                                       int32_t bwA, int32_t bwB, int32_t bwC) {
    auto *c = new uint64_t[dim]();
    auto *d = new uint64_t[dim];
    for (int i = 0; i < dim; i++) {
        c[i] = 0;
        d[i] = 0;
    }
    if (bwA <= bwB) {
        cross_term_reverse(dim, inA, inB, c, bwA, bwB, bwC);
        cross_term(dim, inB, inA, d, bwB, bwA, bwC);
    } else {
        cross_term(dim, inA, inB, c, bwA, bwB, bwC);
        cross_term_reverse(dim, inB, inA, d, bwB, bwA, bwC);
    }
    auto *wx = new uint8_t[dim];
    auto *wy = new uint8_t[dim];
    aux->wrap_computation(inA, wx, dim, bwA);
    aux->wrap_computation(inB, wy, dim, bwB);
    auto *h = new uint64_t[dim];
    auto *g = new uint64_t[dim];
    aux->multiplexer(wx, inB, h, dim, bwB, bwB);
    aux->multiplexer(wy, inA, g, dim, bwA, bwA);
    uint64_t mask = (1ULL << bwC) - 1;
    for (int i = 0; i < dim; i++) {
        outC[i] = (inA[i] * inB[i] + c[i] + d[i] - (g[i] * (1ULL << bwB)) - (h[i] * (1ULL << bwA))) & mask;
    }
    delete[] c;
    delete[] d;
}

void GeometricPerspectiveProtocols::msb0_truncation(int32_t dim, uint64_t *inA, uint64_t *outB, int32_t shift, int32_t bw) {
    if (shift == 0) {
        memcpy(outB, inA, sizeof(uint64_t) * dim);
        return;
    }
    assert((bw - shift) > 0 && "Truncation shouldn't truncate the full bitwidth");
    assert(inA != outB);
    uint64_t mask_bw = (bw == 64 ? -1 : ((1ULL << bw) - 1));
    uint64_t mask_shift = (shift == 64 ? -1 : ((1ULL << shift) - 1));
    uint64_t mask_upper =
            ((bw - shift) == 64 ? -1 : ((1ULL << (bw - shift)) - 1));
    uint64_t *inA_orig = new uint64_t[dim];
    uint64_t *inA_lower = new uint64_t[dim];
    uint64_t *inA_upper = new uint64_t[dim];
    uint8_t *wrap_lower = new uint8_t[dim];
    uint8_t *wrap_upper = new uint8_t[dim];
    uint8_t *eq_upper = new uint8_t[dim];
    uint8_t *and_upper = new uint8_t[dim];
    for (int i = 0; i < dim; i++) {
        inA_lower[i] = inA[i] & mask_shift;
        inA_upper[i] = (inA[i] >> shift) & mask_upper;
    }
    uint8_t *r = new uint8_t[dim];
    uint64_t half = 1ULL << (bw - 1);
    for (int i = 0; i < dim; i++) {
        if (inA[i] >= half) {
            r[i] = 1;
        } else {
            r[i] = 0;
        }
    }
    uint8_t *and_result = new uint8_t[dim];
    uint8_t *dummy = new uint8_t[dim];
    for (int i = 0; i < dim; i++) {
        dummy[i] = 0;
    }
    if (party == sci::ALICE) {
        this->aux->AND(r, dummy, and_result, dim);
    } else {
        this->aux->AND(dummy, r, and_result, dim);
    }
    for (int i = 0; i < dim; i++) {
        and_result[i] = and_result[i] ^ r[i];
    }
    uint64_t* mw = new uint64_t[dim];
    this->aux->B2A(and_result, mw, dim, shift);
    uint64_t *arith_wrap_lower = new uint64_t[dim];
    for (int i = 0; i < dim; i++) {
      arith_wrap_lower[i] = 0;
    }
//    this->aux->wrap_computation(inA_lower, wrap_lower, dim, shift);
//    this->aux->B2A(wrap_lower, arith_wrap_lower, dim, bw);
    for (int i = 0; i < dim; i++) {
        if (party == sci::ALICE) {
            outB[i] = (((inA[i] >> shift) & mask_upper) + arith_wrap_lower[i] - (1ULL << (bw - shift)) * (mw[i])) & mask_bw;
        } else {
            outB[i] = (((inA[i] >> shift) & mask_upper) + arith_wrap_lower[i] - (1ULL << (bw - shift)) * mw[i]) & mask_bw;
        }
    }
    delete[] inA_orig;
    delete[] inA_lower;
    delete[] inA_upper;
    delete[] wrap_lower;
    delete[] wrap_upper;
    delete[] eq_upper;
    delete[] and_upper;
    delete[] arith_wrap_lower;
    return;
}

void GeometricPerspectiveProtocols::new_msb0_truncation(int32_t dim, uint64_t *inA, uint64_t *outB, int32_t shift, int32_t bw) {
    if (shift == 0) {
        memcpy(outB, inA, sizeof(uint64_t) * dim);
        return;
    }
    assert((bw - shift) > 0 && "Truncation shouldn't truncate the full bitwidth");
    assert(inA != outB);
    uint64_t mask_bw = (bw == 64 ? -1 : ((1ULL << bw) - 1));
    uint64_t mask_shift = (shift == 64 ? -1 : ((1ULL << shift) - 1));
    uint64_t mask_upper =
            ((bw - shift) == 64 ? -1 : ((1ULL << (bw - shift)) - 1));

    uint64_t *inA_orig = new uint64_t[dim];
    uint64_t *inA_lower = new uint64_t[dim];
    uint64_t *inA_upper = new uint64_t[dim];
    uint8_t *wrap_lower = new uint8_t[dim];
    uint8_t *wrap_upper = new uint8_t[dim];
    uint8_t *eq_upper = new uint8_t[dim];
    uint8_t *and_upper = new uint8_t[dim];
    for (int i = 0; i < dim; i++) {
        inA_lower[i] = inA[i] & mask_shift;
        inA_upper[i] = (inA[i] >> shift) & mask_upper;
        if (party == sci::BOB) {
            inA_upper[i] = (mask_upper - inA_upper[i]) & mask_upper;
        }
    }
    uint64_t *inA_prime = new uint64_t[dim];
    uint64_t quarter = 1ULL << (bw - 2);
    if (party == sci::ALICE) {
        // x - L/4
        for (int i = 0; i < dim; i++) {
            inA_prime[i] = (inA[i] - quarter) & mask_bw;
        }
    } else {
        for (int i = 0; i < dim; i++) {
            inA_prime[i] = inA[i];
        }
    }
    uint64_t *r = new uint64_t[dim];
    uint64_t half = 1ULL << (bw - 1);
    for (int i = 0; i < dim; i++) {
        if (inA_prime[i] >= half) {
            r[i] = 1;
        } else {
            r[i] = 0;
        }
    }
    uint64_t *bit_mul = new uint64_t[dim];
    if (party == sci::ALICE) {
        uint64_t mask_shift = (1ULL << shift) - 1;
        sci::PRG128 prg;
        uint64_t *data0 = new uint64_t[dim];
        prg.random_data(data0, dim * sizeof(uint64_t));
        otpack->iknp_straight->send_cot(data0, r, dim, shift);
        for (int i = 0; i < dim; i++) {
            bit_mul[i] = ((1ULL << shift) - data0[i]) & mask_shift;
        }
        delete[] data0;
    } else { // party == BOB
        bool *choice = new bool[dim];
        for (int i = 0; i < dim; i++) {
            choice[i] = r[i];
        }
        uint64_t *data = new uint64_t[dim];
        otpack->iknp_straight->recv_cot(data, choice, dim, shift);
        for (int i = 0; i < dim; i++) {
            bit_mul[i] = data[i];
        }
        delete[] choice;
    }
    uint64_t offset = 1ULL << (bw - shift - 2);
    for (int i = 0; i < dim; i++) {
        if (party == sci::ALICE) {
            outB[i] = (((inA_prime[i] >> shift) & mask_upper) + offset -
                       (1ULL << (bw - shift)) * (bit_mul[i] + 1)) &
                      mask_bw;
        } else {
            outB[i] = (((inA_prime[i] >> shift) & mask_upper) -
                       (1ULL << (bw - shift)) * bit_mul[i]) &
                      mask_bw;
        }
    }
    delete[] inA_orig;
    delete[] inA_lower;
    delete[] inA_upper;
    delete[] wrap_lower;
    delete[] wrap_upper;
    delete[] eq_upper;
    delete[] and_upper;
    return;
}