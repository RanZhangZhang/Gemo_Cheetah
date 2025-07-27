 #include "BuildingBlocks/geometric_perspective_protocols.h"
#include "BuildingBlocks/value-extension.h"
#include "BuildingBlocks/truncation.h"
#include <iostream>

using namespace sci;
using namespace std;

int dim, bwA, bwB, shift;
// vars
int party, port = 32000;
string address = "127.0.0.1";
NetIO *io;
OTPack<NetIO> *otpack;
GeometricPerspectiveProtocols *geom;
Truncation *trunc_oracle;
AuxProtocols *aux;
XTProtocol *xtProtocol;
PRG128 prg;

void sirnn_unsigned_mul() {
    uint64_t *inA = new uint64_t[dim];
    uint64_t *inB = new uint64_t[dim];
    uint64_t *outC = new uint64_t[dim];
    prg.random_data(inA, dim * sizeof(uint64_t));
    prg.random_data(inB, dim * sizeof(uint64_t));
    uint64_t mask_bwA = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    uint64_t mask_bwB = (bwB == 64 ? -1 : ((1ULL << bwB) - 1));
    for (int i = 0; i < dim; i++) {
        inA[i] = inA[i] & mask_bwA;
        inB[i] = inB[i] & mask_bwB;
        outC[i] = 0;
    }
    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();
    geom->sirnn_unsigned_mul(dim, inA, inB, outC, bwA, bwB, bwA + bwB);
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for SirNN mul = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "unsigned mul " << bwA << " " << bwB << " bits ints. Sent " << ((c1 - c0) * 8 / dim) << " bits\n";
    if (party == sci::ALICE) {
        int count = 0;
        uint64_t mask_out = (1ULL << (bwA + bwB)) - 1;
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t *inB_bob = new uint64_t[dim];
        uint64_t *res = new uint64_t[dim];
        uint64_t *outC_bob = new uint64_t[dim];
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        io->recv_data(inB_bob, sizeof(uint64_t) * dim);
        io->recv_data(outC_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            inA[i] = (inA[i] + inA_bob[i]) & mask_bwA;
            inB[i] = (inB[i] + inB_bob[i]) & mask_bwB;
            res[i] = (inA[i] * inB[i]) & mask_out;
            outC[i] = (outC[i] + outC_bob[i]) & mask_out;
        }
        cout << "Testing for correctness..." << endl;
        for (int i = 0; i < dim; i++) {
            assert(res[i] == outC[i]);
            if (res[i] == outC[i]) {
                count++;
            }
        }
        cout << count << " / " << dim << " Correct!" << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        io->send_data(inB, sizeof(uint64_t) * dim);
        io->send_data(outC, sizeof(uint64_t) * dim);
    }
}

void geom_signed_mul() {
    uint64_t *inA = new uint64_t[dim];
    uint64_t *inB = new uint64_t[dim];
    uint64_t mask_bwA = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    uint64_t mask_bwB = (bwB == 64 ? -1 : ((1ULL << bwB) - 1));
    if (party == sci::ALICE) {
        uint64_t *inputA = new uint64_t[dim];
        uint64_t *inputB = new uint64_t[dim];
        uint64_t maskA = (1ULL << (bwA - 2)) - 1;
        uint64_t maskB = (1ULL << (bwB - 2)) - 1;
        prg.random_data(inputA, dim * sizeof(uint64_t));
        prg.random_data(inputB, dim * sizeof(uint64_t));
        for (int i = 0; i < dim; i++) {
            inputA[i] = inputA[i] & maskA;
            inputB[i] = inputB[i] & maskB;
        }
        int size1 = dim / 4;
        int size2 = dim / 2;
        int size3 = dim * 3 / 4;
        for (int i = 0; i < size1; i++) {
            inputA[i] = inputA[i];
            inputB[i] = inputB[i];
        }
        for (int i = size1; i < size2; i++) {
            inputA[i] = inputA[i];
            inputB[i] = (1ULL << bwB) - inputB[i];
        }
        for (int i = size2; i < size3; i++) {
            inputA[i] = (1ULL << bwA) - inputA[i];
            inputB[i] = inputB[i];
        }
        for (int i = size3; i < dim; i++) {
            inputA[i] = (1ULL << bwA) - inputA[i];
            inputB[i] = (1ULL << bwB) - inputB[i];
        }
        prg.random_data(inA, dim * sizeof(uint64_t));
        prg.random_data(inB, dim * sizeof(uint64_t));
        for (int i = 0; i < dim; i++) {
            inA[i] = inA[i] & mask_bwA;
            inB[i] = inB[i] & mask_bwB;
        }
        uint64_t *bob_inputA = new uint64_t[dim];
        uint64_t *bob_inputB = new uint64_t[dim];
        for (int i = 0; i < dim; i++) {
            bob_inputA[i] = (inputA[i] - inA[i]) & mask_bwA;
            bob_inputB[i] = (inputB[i] - inB[i]) & mask_bwB;
        }
        io->send_data(bob_inputA, sizeof(uint64_t) * dim);
        io->send_data(bob_inputB, sizeof(uint64_t) * dim);
    } else {
        io->recv_data(inA, sizeof(uint64_t) * dim);
        io->recv_data(inB, sizeof(uint64_t) * dim);
    }
    uint64_t *outC = new uint64_t[dim];
    uint64_t mask_out = (1ULL << (bwA + bwB)) - 1;
    for (int i = 0; i < dim; i++) {
        outC[i] = 0;
    }
    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();
    geom->signed_mul(dim, inA, inB, outC, bwA, bwB, bwA + bwB);
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for Geometric perspective mul = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "Geometric Perspective signed mul " << bwA << " " << bwB << " bits ints. Sent " << ((c1 - c0) * 8 / dim) << " bits\n";
    if (party == sci::ALICE) {
        int count = 0;
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t *inB_bob = new uint64_t[dim];
        uint64_t *res = new uint64_t[dim];
        uint64_t *outC_bob = new uint64_t[dim];
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        io->recv_data(inB_bob, sizeof(uint64_t) * dim);
        io->recv_data(outC_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            inA[i] = (inA[i] + inA_bob[i]) & mask_bwA;
            inB[i] = (inB[i] + inB_bob[i]) & mask_bwB;
            res[i] = (signed_val(inA[i], bwA) * signed_val(inB[i], bwB)) & mask_out;
            outC[i] = (outC[i] + outC_bob[i]) & mask_out;
        }
        cout << "Testing for correctness..." << endl;
        for (int i = 0; i < dim; i++) {
            assert(res[i] == outC[i]);
            if (res[i] == outC[i]) {
                count++;
            }
        }
        cout << count << " / " << dim << " Correct!" << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        io->send_data(inB, sizeof(uint64_t) * dim);
        io->send_data(outC, sizeof(uint64_t) * dim);
    }
}

void geom_sign_extension() {
    uint64_t *inA = new uint64_t[dim];
    uint64_t *outB = new uint64_t[dim];
    uint64_t mask_bw = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    if (party == sci::ALICE) {
        uint64_t *inputA = new uint64_t[dim];
        uint64_t mask = (1ULL << (bwA - 2)) - 1;
        prg.random_data(inputA, dim * sizeof(uint64_t));
        for (int i = 0; i < dim; i++) {
            inputA[i] = inputA[i] & mask;
        }
        int size = dim / 2;
        for (int i = size; i < dim; i++) {
          inputA[i] = (1ULL << bwA) - inputA[i];
        }
        prg.random_data(inA, dim * sizeof(uint64_t));
        for (int i = 0; i < dim; i++) {
            inA[i] &= mask_bw;
            outB[i] = 0;
        }
        uint64_t *bob_input = new uint64_t[dim];
        for (int i = 0; i < dim; i++) {
            bob_input[i] = (inputA[i] - inA[i]) & mask_bw;
        }
        io->send_data(bob_input, sizeof(uint64_t) * dim);
    } else {
        io->recv_data(inA, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            outB[i] = 0;
        }
    }
    uint64_t mask_out = (bwB == 64 ? -1 : ((1ULL << bwB) - 1));
    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();
    geom->sign_extension(dim, inA, outB, bwA, bwB);
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for Geometric Perspective value extension = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "Geometric Perspective sign extension " << bwA << " " << bwB << " bits ints bits. Sent " << ((c1 - c0) * 8 / dim) << " bits\n";
    if (party == sci::ALICE) {
        int count = 0;
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t *outB_bob = new uint64_t[dim];
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        io->recv_data(outB_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            inA[i] = (inA[i] + inA_bob[i]) & mask_bw;
            outB[i] = (outB[i] + outB_bob[i]) & mask_out;
        }
        cout << "Testing for correctness..." << endl;
        for (int i = 0; i < dim; i++) {
            if (signed_val(outB[i], bwB) == signed_val(inA[i], bwA)) {
                count++;
            }
        }
        cout << count << " / " << dim << " Correct!" << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        io->send_data(outB, sizeof(uint64_t) * dim);
    }
}

void sirnn_sign_extension() {
    uint64_t *inA = new uint64_t[dim];
    uint64_t *outB = new uint64_t[dim];
    uint64_t mask_bw = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    prg.random_data(inA, dim * sizeof(uint64_t));
    for (int i = 0; i < dim; i++) {
        inA[i] &= mask_bw;
        outB[i] = 0;
    }
    uint64_t mask_out = (bwB == 64 ? -1 : ((1ULL << bwB) - 1));
    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();
    xtProtocol->s_extend(dim, inA, outB, bwA, bwB);
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for SirNN value extension = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "SirNN sign extension " << bwA << " " << bwB << " bits ints bits. Sent " << ((c1 - c0) * 8 / dim) << " bits\n";
    if (party == sci::ALICE) {
        int count = 0;
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t *outB_bob = new uint64_t[dim];
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        io->recv_data(outB_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            inA[i] = (inA[i] + inA_bob[i]) & mask_bw;
            outB[i] = (outB[i] + outB_bob[i]) & mask_out;
        }
        cout << "Testing for correctness..." << endl;
        for (int i = 0; i < dim; i++) {
            if (signed_val(outB[i], bwB) == signed_val(inA[i], bwA)) {
                count++;
            }
        }
        cout << count << " / " << dim << " Correct!" << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        io->send_data(outB, sizeof(uint64_t) * dim);
    }
}

void geom_trunc() {
    uint64_t *inA = new uint64_t[dim];
    uint64_t *outB = new uint64_t[dim];
    uint64_t mask_bw = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    if (party == sci::ALICE) {
        uint64_t *inputA = new uint64_t[dim];
        uint64_t mask = (1ULL << (bwA - 2)) - 1;
        prg.random_data(inputA, dim * sizeof(uint64_t));
        for (int i = 0; i < dim; i++) {
            inputA[i] = inputA[i] & mask;
        }
        int size = dim / 2;
        for (int i = size; i < dim; i++) {
            inputA[i] = (1ULL << bwA) - inputA[i];
        }
        prg.random_data(inA, dim * sizeof(uint64_t));
        for (int i = 0; i < dim; i++) {
            inA[i] &= mask_bw;
            outB[i] = 0;
        }
        uint64_t *bob_input = new uint64_t[dim];
        for (int i = 0; i < dim; i++) {
            bob_input[i] = (inputA[i] - inA[i]) & mask_bw;
        }
        io->send_data(bob_input, sizeof(uint64_t) * dim);
    } else {
        io->recv_data(inA, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            outB[i] = 0;
        }
    }
    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();
    geom->new_truncate(dim, inA, outB, shift, bwA);
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for Geometric Perspective Protocol = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "truncate " << "signed" << " " << bwA << " bits ints ";
    std::cout <<  "shift by " << shift << " bits. Sent " << ((c1 - c0) * 8 / dim) << " bits\n";
    if (party == sci::ALICE) {
        int count = 0;
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t *outB_bob = new uint64_t[dim];
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        io->recv_data(outB_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            inA[i] = (inA[i] + inA_bob[i]) & mask_bw;
            outB[i] = (outB[i] + outB_bob[i]) & mask_bw;
        }
        cout << "Testing for correctness..." << endl;
        for (int i = 0; i < dim; i++) {
            assert((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA));
            if ((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA)) {
                count++;
            }
        }
        cout << count << " / " << dim << " Correct!" << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        io->send_data(outB, sizeof(uint64_t) * dim);
    }
}

void trunc(bool signed_arithmetic = true) {
    uint64_t mask_bw = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    uint64_t *inA = new uint64_t[dim];
    uint64_t *outB = new uint64_t[dim];
    prg.random_data(inA, dim * sizeof(uint64_t));
    for (int i = 0; i < dim; i++) {
        inA[i] &= mask_bw;
        outB[i] = 0;
    }
    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();
    trunc_oracle->truncate(dim, inA, outB, shift, bwA, signed_arithmetic);
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for SirNN = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "truncate " << (signed_arithmetic ? "signed" : "nonsigned") << " " << bwA << " bits ints ";
    std::cout <<  "shift by " << shift << " bits. Sent " << ((c1 - c0) * 8 / dim) << " bits\n";
    if (party == sci::ALICE) {
        int count = 0;
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t *outB_bob = new uint64_t[dim];
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        io->recv_data(outB_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            inA[i] = (inA[i] + inA_bob[i]) & mask_bw;
            outB[i] = (outB[i] + outB_bob[i]) & mask_bw;
        }
        cout << "Testing for correctness..." << endl;
        for (int i = 0; i < dim; i++) {
            if (signed_arithmetic) {
                if ((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA)) {
                    count++;
                }
            } else {
                if ((inA[i] >> shift) == outB[i]) {
                    count++;
                }
            }
        }
        cout << count << " / " << dim << " Correct!" << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        io->send_data(outB, sizeof(uint64_t) * dim);
    }
}


void trunc_with_one_bit_error(bool signed_arithmetic = true) {
    uint64_t mask_bw = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    uint64_t *inA = new uint64_t[dim];
    uint64_t *outB = new uint64_t[dim];
    prg.random_data(inA, dim * sizeof(uint64_t));
    for (int i = 0; i < dim; i++) {
        inA[i] &= mask_bw;
        outB[i] = 0;
    }
    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();
    trunc_oracle->truncate_with_one_bit_error(dim, inA, outB, shift, bwA, signed_arithmetic);
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for SirNN = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "truncate " << (signed_arithmetic ? "signed" : "nonsigned") << " " << bwA << " bits ints ";
    std::cout <<  "shift by " << shift << " bits. Sent " << ((c1 - c0) * 8 / dim) << " bits\n";
    if (party == sci::ALICE) {
        int count = 0;
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t *outB_bob = new uint64_t[dim];
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        io->recv_data(outB_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            inA[i] = (inA[i] + inA_bob[i]) & mask_bw;
            outB[i] = (outB[i] + outB_bob[i]) & mask_bw;
        }
        cout << "Testing for correctness..." << endl;
        for (int i = 0; i < dim; i++) {
            if (signed_arithmetic) {
                if ((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA)) {
                    count++;
                }
                if ((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA) + 1) {
                    count++;
                }
                if ((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA) - 1) {
                    count++;
                }
            } else {
                if ((inA[i] >> shift) == outB[i]) {
                    count++;
                }
            }
        }
        cout << count << " / " << dim << " Correct!" << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        io->send_data(outB, sizeof(uint64_t) * dim);
    }
}

void msb0_trunc() {
    uint64_t mask_bw = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    uint64_t *inA = new uint64_t[dim];
    uint64_t *outB = new uint64_t[dim];
    prg.random_data(inA, dim * sizeof(uint64_t));
    for (int i = 0; i < dim; i++) {
        inA[i] &= mask_bw;
        outB[i] = 0;
    }
    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();
    geom->msb0_truncation(dim, inA, outB, shift, bwA);
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for Cheetah = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "truncate " << " " << bwA << " bits ints ";
    std::cout <<  "shift by " << shift << " bits. Sent " << ((c1 - c0) * 8 / dim) << " bits\n";
    if (party == sci::ALICE) {
        int count = 0;
        int total = 0;
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t *outB_bob = new uint64_t[dim];
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        io->recv_data(outB_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            inA[i] = (inA[i] + inA_bob[i]) & mask_bw;
            outB[i] = (outB[i] + outB_bob[i]) & mask_bw;
        }
        cout << "Testing for correctness..." << endl;
        for (int i = 0; i < dim; i++) {
            if (inA[i] < (1ULL << (bwA - 2))) {
                total++;
                if ((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA)) {
                    count++;
                }
                if ((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA) + 1) {
                    count++;
                }
                if ((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA) - 1) {
                    count++;
                }
            }
        }
        cout << count << " / " << total << " Correct!" << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        io->send_data(outB, sizeof(uint64_t) * dim);
    }
}

void geom_trunc_with_one_bit_error() {
    uint64_t *inA = new uint64_t[dim];
    uint64_t *outB = new uint64_t[dim];
    uint64_t mask_bw = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    if (party == sci::ALICE) {
        uint64_t *inputA = new uint64_t[dim];
        uint64_t mask = (1ULL << (bwA - 2)) - 1;
        prg.random_data(inputA, dim * sizeof(uint64_t));
        for (int i = 0; i < dim; i++) {
            inputA[i] = inputA[i] & mask;
        }
        int size = dim / 2;
        for (int i = size; i < dim; i++) {
            inputA[i] = (1ULL << bwA) - inputA[i];
        }
        prg.random_data(inA, dim * sizeof(uint64_t));
        for (int i = 0; i < dim; i++) {
            inA[i] &= mask_bw;
            outB[i] = 0;
        }
        uint64_t *bob_input = new uint64_t[dim];
        for (int i = 0; i < dim; i++) {
            bob_input[i] = (inputA[i] - inA[i]) & mask_bw;
        }
        io->send_data(bob_input, sizeof(uint64_t) * dim);
    } else {
        io->recv_data(inA, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            outB[i] = 0;
        }
    }
    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();
    geom->truncate_with_one_bit_error(dim, inA, outB, shift, bwA);
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for Geometric Perspective truncation with one bit error = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "truncate " << " " << bwA << " bits ints ";
    std::cout <<  "shift by " << shift << " bits. Sent " << ((c1 - c0) * 8 / dim) << " bits\n";
    if (party == sci::ALICE) {
        int count = 0;
        int total = 0;
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t *outB_bob = new uint64_t[dim];
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        io->recv_data(outB_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            inA[i] = (inA[i] + inA_bob[i]) & mask_bw;
            outB[i] = (outB[i] + outB_bob[i]) & mask_bw;
        }
        cout << "Testing for correctness..." << endl;
        for (int i = 0; i < dim; i++) {
            if ((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA)) {
                count++;
            }
            if ((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA) + 1) {
                count++;
            }
            if ((signed_val(inA[i], bwA) >> shift) == signed_val(outB[i], bwA) - 1) {
                count++;
            }
        }
        cout << count << " / " << dim << " Correct!" << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        io->send_data(outB, sizeof(uint64_t) * dim);
    }
}

void geom_msb0_trunc() {
    uint64_t *inA = new uint64_t[dim];
    uint64_t *outB = new uint64_t[dim];
    uint64_t mask_bw = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    prg.random_data(inA, dim * sizeof(uint64_t));
    for (int i = 0; i < dim; i++) {
        inA[i] &= mask_bw;
        outB[i] = 0;
    }
    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();
    geom->new_msb0_truncation(dim, inA, outB, shift, bwA);
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for Geometric Perspective truncation with msb0 = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "truncate " << " " << bwA << " bits ints ";
    std::cout <<  "shift by " << shift << " bits. Sent " << ((c1 - c0) * 8 / dim) << " bits\n";
    if (party == sci::ALICE) {
        int count = 0;
        int total = 0;
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t *outB_bob = new uint64_t[dim];
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        io->recv_data(outB_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < dim; i++) {
            inA[i] = (inA[i] + inA_bob[i]) & mask_bw;
            outB[i] = (outB[i] + outB_bob[i]) & mask_bw;
        }
        cout << "Testing for correctness..." << endl;
        for (int i = 0; i < dim; i++) {
            if (inA[i] < (1ULL << (bwA - 2))) {
                total++;
                if (outB[i] + 1 == (inA[i] >> shift)) {
                    count++;
                }
                if (outB[i] - 1 == (inA[i] >> shift)) {
                    count++;
                }
                if (outB[i] == (inA[i] >> shift)) {
                    count++;
                }
            }
        }
        cout << count << " / " << total << " Correct!" << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        io->send_data(outB, sizeof(uint64_t) * dim);
    }
}

void test_sirnn_euclidean_distance() {
	int size = 10;
    uint64_t *inA = new uint64_t[dim];
    uint64_t **inB = new uint64_t*[size];
    for (int i = 0; i < size; i++) {
        inB[i] = new uint64_t[dim];
    }
    prg.random_data(inA, dim * sizeof(uint64_t));
    for (int i = 0; i < size; i++) {
        prg.random_data(inB[i], dim * sizeof(uint64_t));
    }
    uint64_t mask_bwA = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    for (int i = 0; i < dim; i++) {
        inA[i] = inA[i] & mask_bwA;
    }
    for (int i = 0; i < size; i++) {
      for (int j = 0; j < dim; j++) {
        inB[i][j] = inB[i][j] & mask_bwA;
      }
    }
    uint64_t mask = (1ULL << (bwA)) - 1;
    int32_t extra_bits = ceil(log2(dim));
    uint64_t out_mask = (1ULL << (bwA + extra_bits)) - 1;
    // Radix base = 4
    ReLURingProtocol<sci::NetIO, uint64_t> *relu_oracle = new ReLURingProtocol<sci::NetIO, uint64_t>(party, RING, io, bwA + extra_bits, 4, otpack);

    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();

    uint64_t* distance = new uint64_t[size];
    for (int i = 0; i < size; i++) {
      distance[i] = 0;
      uint64_t* c = new uint64_t[dim];
      uint64_t* c1 = new uint64_t[dim];
      uint64_t* c2 = new uint64_t[dim];
      geom->sirnn_unsigned_mul(dim, inA, inB[i], c, bwA, bwA, bwA + bwA);
	  trunc_oracle->truncate(dim, c, c1, bwA, bwA + bwA, false);
      xtProtocol->z_extend(dim, c1, c1, bwA, bwA + extra_bits);
      for (int j = 0; j < dim; j++) {
        distance[i] = (distance[i] + c1[j]) & out_mask;
      }
    }
    uint64_t outC = distance[0];
    for (int i = 1; i < size; i++) {
      uint64_t d = (distance[i] - outC) & out_mask;
      uint64_t d1 = (outC - distance[i]) & out_mask;
      uint64_t sel = 0;
      uint8_t drelu = 0;
      relu_oracle->relu(&outC, &d1, 1, &drelu, true);
      aux->multiplexer(&drelu, &d1, &sel, 1, bwA + extra_bits, bwA + extra_bits);
      outC = (sel + distance[i]) & out_mask;
    }

    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for Euclidean distance = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "Euclidean distance test " << bwA + extra_bits << " bits ints. Sent " << ((c1 - c0) * 8) << " bits\n";

    if (party == sci::ALICE) {
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t **inB_bob = new uint64_t*[size];
        for (int i = 0; i < size; i++) {
          inB_bob[i] = new uint64_t[dim];
        }
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < size; i++) {
            io->recv_data(inB_bob[i], sizeof(uint64_t) * dim);
        }
        uint64_t *outC_bob = new uint64_t[1];
        geom->io->recv_data(outC_bob, sizeof(uint64_t) * 1);
        outC = (outC_bob[0] + outC) & out_mask;
        uint64_t res = -1;
        for (int i = 0; i < size; i++) {
            uint64_t temp_res = 0;
            for (int j = 0; j < dim; j++) {
                uint64_t a = (inA[j] + inA_bob[j]) & mask_bwA;
                uint64_t b = (inB[i][j] + inB_bob[i][j]) & mask_bwA;
                temp_res = (temp_res + ((a * b) >> bwA)) & out_mask;
            }
            if (temp_res < res) {
              res = temp_res;
            }
        }
        cout << "Testing for correctness..." << endl;
        cout << "expect distance is " << res << endl;
        cout << "actual distance is " << outC << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        for (int i = 0; i < size; i++) {
            io->send_data(inB[i], sizeof(uint64_t) * dim);
        }
        uint64_t *inA_bob = new uint64_t[1];
        inA_bob[0] = outC;
        io->send_data(inA_bob, sizeof(uint64_t) * 1);
    }
}

void test_geom_euclidean_distance() {
	int size = 10;
    uint64_t *inA = new uint64_t[dim];
    uint64_t **inB = new uint64_t*[size];
    for (int i = 0; i < size; i++) {
        inB[i] = new uint64_t[dim];
    }
    if (party == sci::ALICE) {
        uint64_t *inputA = new uint64_t[dim];
        uint64_t **inputB = new uint64_t*[size];
        for (int i = 0; i < size; i++) {
        	inputB[i] = new uint64_t[dim];
    	}
        uint64_t maskA = (1ULL << (bwA - 2)) - 1;
        prg.random_data(inputA, dim * sizeof(uint64_t));
        for (int i = 0; i < size; i++) {
            prg.random_data(inputB[i], dim * sizeof(uint64_t));
        }
        for (int i = 0; i < dim; i++) {
            inputA[i] = inputA[i] & maskA;
        }
        for (int i = 0; i < size; i++) {
          for (int j = 0; j < dim; j++) {
            inputB[i][j] = inputB[i][j] & maskA;
          }
        }
        prg.random_data(inA, dim * sizeof(uint64_t));
        for (int i = 0; i < size; i++) {
          prg.random_data(inB[i], dim * sizeof(uint64_t));
        }
        for (int i = 0; i < dim; i++) {
            inA[i] = inA[i] & maskA;
        }
        for (int i = 0; i < size; i++) {
          for (int j = 0; j < dim; j++) {
            inB[i][j] = inB[i][j] & maskA;
          }
        }
        uint64_t *bob_inputA = new uint64_t[dim];
        uint64_t **bob_inputB = new uint64_t*[size];
        for (int i = 0; i < dim; i++) {
            bob_inputA[i] = (inputA[i] - inA[i]) & maskA;
        }
        for (int i = 0; i < size; i++) {
          bob_inputB[i] = new uint64_t[dim];
          for (int j = 0; j < dim; j++) {
            bob_inputB[i][j] = (inputB[i][j] - inB[i][j]) & maskA;
          }
        }
        io->send_data(bob_inputA, sizeof(uint64_t) * dim);
        for (int i = 0; i < size; i++) {
          io->send_data(bob_inputB[i], sizeof(uint64_t) * dim);
        }
    } else {
        io->recv_data(inA, sizeof(uint64_t) * dim);
        for (int i = 0; i < size; i++) {
          io->recv_data(inB[i], sizeof(uint64_t) * dim);
        }
    }
    uint64_t mask_bwA = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
    uint64_t mask = (1ULL << (bwA)) - 1;
    int32_t extra_bits = ceil(log2(dim));
    uint64_t out_mask = (1ULL << (bwA + extra_bits)) - 1;
    // Radix base = 4
    ReLURingProtocol<sci::NetIO, uint64_t> *relu_oracle = new ReLURingProtocol<sci::NetIO, uint64_t>(party, RING, io, bwA + extra_bits, 4, otpack);

    int64_t c0 = io->counter;
    auto start_timer = std::chrono::high_resolution_clock::now();
    uint64_t* distance = new uint64_t[size];
    for (int i = 0; i < size; i++) {
      distance[i] = 0;
      uint64_t* c = new uint64_t[dim];
      uint64_t* c1 = new uint64_t[dim];
      uint64_t* c2 = new uint64_t[dim];
      geom->signed_mul(dim, inA, inB[i], c, bwA, bwA, bwA + bwA);
      geom->new_truncate(dim, c, c1, bwA, bwA + bwA);
      xtProtocol->z_extend(dim, c1, c2, bwA, bwA + extra_bits);
      for (int j = 0; j < dim; j++) {
        distance[i] = (distance[i] + c2[j]) & out_mask;
      }
    }
    uint64_t outC = distance[0];
    for (int i = 1; i < size; i++) {
      uint64_t d = (distance[i] - outC) & out_mask;
      uint64_t d1 = (outC - distance[i]) & out_mask;
      uint64_t sel = 0;
      uint8_t drelu = 0;
      relu_oracle->relu(&outC, &d1, 1, &drelu, true);
      aux->multiplexer(&drelu, &d1, &sel, 1, bwA + extra_bits, bwA + extra_bits);
      outC = (sel + distance[i]) & out_mask;
    }
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for Euclidean distance = " << (temp / 1000.0) << std::endl;
    int64_t c1 = io->counter;
    std::cout << "Euclidean distance test " << bwA + extra_bits << " bits ints. Sent " << ((c1 - c0) * 8) << " bits\n";

    if (party == sci::ALICE) {
        uint64_t *inA_bob = new uint64_t[dim];
        uint64_t **inB_bob = new uint64_t*[size];
        for (int i = 0; i < size; i++) {
          inB_bob[i] = new uint64_t[dim];
        }
        io->recv_data(inA_bob, sizeof(uint64_t) * dim);
        for (int i = 0; i < size; i++) {
            io->recv_data(inB_bob[i], sizeof(uint64_t) * dim);
        }
        uint64_t *outC_bob = new uint64_t[1];
        io->recv_data(outC_bob, sizeof(uint64_t) * 1);
        outC = (outC_bob[0] + outC) & out_mask;
        uint64_t res = -1;
        for (int i = 0; i < size; i++) {
            uint64_t temp_res = 0;
            for (int j = 0; j < dim; j++) {
                uint64_t a = (inA[j] + inA_bob[j]) & mask_bwA;
                uint64_t b = (inB[i][j] + inB_bob[i][j]) & mask_bwA;
                temp_res = (temp_res + ((a * b) >> bwA)) & out_mask;
            }
            if (temp_res < res) {
              res = temp_res;
            }
        }
        cout << "Testing for correctness..." << endl;
        cout << "expect distance is " << res << endl;
        cout << "actual distance is " << outC << endl;
    } else { // BOB
        io->send_data(inA, sizeof(uint64_t) * dim);
        for (int i = 0; i < size; i++) {
            io->send_data(inB[i], sizeof(uint64_t) * dim);
        }
        uint64_t *inA_bob = new uint64_t[1];
        inA_bob[0] = outC;
        io->send_data(inA_bob, sizeof(uint64_t) * 1);
    }
}

int main(int argc, char **argv) {
    // default
    dim = 1 << 16;
    bwA = 25;
    bwB = 30;
    shift = 12;
    ArgMapping amap;
    amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
    amap.arg("p", port, "Port Number");
    amap.arg("N", dim, "Number of operations");
    amap.arg("bwA", bwA, "Bitlength of inputA");
    amap.arg("bwB", bwB, "Bitlength of inputB");
    amap.arg("s", shift, "Bitlength of shift");
    amap.arg("ip", address, "IP Address of server (ALICE)");

    amap.parse(argc, argv);

    auto start_timer = std::chrono::high_resolution_clock::now();

    io = new NetIO(party == 1 ? nullptr : address.c_str(), port);
    otpack = new OTPack<NetIO>(io, party);
    geom = new GeometricPerspectiveProtocols(party, io, otpack);
    trunc_oracle = new Truncation(party, io, otpack);
    aux = new AuxProtocols(party, io, otpack);
    xtProtocol = new XTProtocol(party, io, otpack);
    auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    std::cout << "Time in sec for preprocessing = " << (temp / 1000.0) << std::endl;

    cout << "<><><><> test geometric perspective faithful truncation <><><><>" << endl;
    for (int i = 0; i < 5; i++) {
        io->sync();
        geom_trunc();
        io->sync();
    }
    for (int i = 0; i < 5; i++) {
        io->sync();
        trunc();
        io->sync();
    }

//    cout << "<><><><> test geometric perspective signed multiplication <><><><>" << endl;
//    for (int i = 0; i < 5; i++) {
//        io->sync();
//        geom_signed_mul();
//        io->sync();
//    }
//    for (int i = 0; i < 5; i++) {
//        io->sync();
//        sirnn_unsigned_mul();
//        io->sync();
//    }
//
//    cout << "<><><><> test geometric perspective value extension <><><><>" << endl;
//    for (int i = 0; i < 5; i++) {
//        io->sync();
//        geom_sign_extension();
//        io->sync();
//    }
//    for (int i = 0; i < 5; i++) {
//        io->sync();
//        sirnn_sign_extension();
//        io->sync();
//    }
//
//    cout << "<><><><> test geometric perspective truncation with one bit error <><><><>" << endl;
//    for (int i = 0; i < 5; i++) {
//        io->sync();
//        geom_trunc_with_one_bit_error();
//        io->sync();
//    }
//    for (int i = 0; i < 5; i++) {
//        io->sync();
//        trunc_with_one_bit_error();
//        io->sync();
//    }
//
//    cout << "<><><><> test geometric perspective truncation with msb0 <><><><>" << endl;
//    for (int i = 0; i < 5; i++) {
//        io->sync();
//        geom_msb0_trunc();
//        io->sync();
//    }
//    for (int i = 0; i < 5; i++) {
//        io->sync();
//        msb0_trunc();
//        io->sync();
//    }


//    cout << "<><><><> test Euclidean distance <><><><>" << endl;
//    for (int i = 0; i < 5; i++) {
//        io->sync();
//        test_sirnn_euclidean_distance();
//        io->sync();
//    }
//    for (int i = 0; i < 5; i++) {
//        io->sync();
//        test_geom_euclidean_distance();
//        io->sync();
//    }
}
