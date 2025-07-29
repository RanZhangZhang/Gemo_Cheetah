// Added by Ran Zhang at 2025/7/3
// To test geometric_perspective_protocols
#include <iostream>
#include "library_fixed.h"
#include "BuildingBlocks/geometric_perspective_protocols.h"
#include "globals.h" 


using namespace std;

int dim, bwA, shift;
int bitlength = 37;
int num_threads = 1;
int kScale = 12;

int party, port = 32000;
string address = "127.0.0.1";
sci::PRG128 prg;
GeometricPerspectiveProtocols *geom;

void geom_trunc() {
    uint64_t *inA = new uint64_t[dim];

    uint64_t *outB = new uint64_t[dim];
    uint64_t mask_bw = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));    
    if (party == SERVER) {
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
    // int64_t c0 = io->counter;
    // auto start_timer = std::chrono::high_resolution_clock::now();

    geom->new_truncate(dim, inA, outB, shift, bwA);

    // auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-start_timer).count();
    // std::cout << "Time in sec for Geometric Perspective Protocol = " << (temp / 1000.0) << std::endl;
    // int64_t c1 = io->counter;
    // std::cout << "truncate " << "signed" << " " << bwA << " bits ints ";
    // std::cout <<  "shift by " << shift << " bits. Sent " << ((c1 - c0) * 8 / dim) << " bits\n";
    
    //-------------------------- Check correctness ---------------------------
    // if (party == SERVER) {
    //     int count = 0;
    //     uint64_t *inA_bob = new uint64_t[dim];
    //     uint64_t *outB_bob = new uint64_t[dim];
    //     io->recv_data(inA_bob, sizeof(uint64_t) * dim);
    //     io->recv_data(outB_bob, sizeof(uint64_t) * dim);

    //     for (int i = 0; i < dim; i++) {
    //         inA[i] = (inA[i] + inA_bob[i]) & mask_bw;
    //         outB[i] = (outB[i] + outB_bob[i]) & mask_bw;
    //     }
    //     cout << "Testing for correctness..." << endl;
    //     for (int i = 0; i < dim; i++) {
    //         assert((sci::signed_val(inA[i], bwA) >> shift) == sci::signed_val(outB[i], bwA));
    //         if ((sci::signed_val(inA[i], bwA) >> shift) == sci::signed_val(outB[i], bwA)) {
    //             count++;
    //         }
    //     }
    //     cout << count << " / " << dim << " Correct!" << endl;

    // } else { // CLIENT
    //     io->send_data(inA, sizeof(uint64_t) * dim);
    //     io->send_data(outB, sizeof(uint64_t) * dim);
    // }
    //------------------------- Check over --------------------------------
}

int main(int argc, char **argv) {
    // default values
    dim = 10000; //相当于size
    bwA = 37; // 相当于l, 环的大小
    shift = 12;
    
    ArgMapping amap;
    amap.arg("r", party, "Role of party: ALICE/SERVER = 1; BOB/CLIENT = 2");
    amap.arg("p", port, "Port Number");
    amap.arg("ip", address, "IP Address of server (ALICE)");
    amap.arg("nt", num_threads, "Number of Threads");
    amap.arg("ell", bitlength, "Uniform Bitwidth");
    amap.arg("k", kScale, "bits of scale");

    amap.parse(argc, argv);

    assert(party == SERVER || party == CLIENT);

    StartComputation();

    geom = new GeometricPerspectiveProtocols(party, io, otpack);
    
    // 检查 geom 是否正确初始化
    if (geom == nullptr) {
        cerr << "Error: geom pointer is null! Make sure USE_CHEETAH is defined and StartComputation() was called." << endl;
        return -1;
    }
    
    cout << "<><><><> test 1geometric perspective faithful truncation <><><><>" << endl;
   
    geom_trunc();

    EndComputation();

    finalize();
    
}