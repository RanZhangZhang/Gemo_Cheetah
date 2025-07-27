// anonymous authors

#ifndef GEOMTRIC_PERSPECTIVE_PROTOCOLS_H
#define GEOMTRIC_PERSPECTIVE_PROTOCOLS_H

#include "BuildingBlocks/aux-protocols.h"
#include "Millionaire/equality.h"
#include "Millionaire/millionaire.h"
#include "Millionaire/millionaire_with_equality.h"
#include "NonLinear/relu-ring.h"
#include "OT/emp-ot.h"

class GeometricPerspectiveProtocols {

    public:
        // sci::NetIO *io = nullptr;
        sci::NetIO *io;
        sci::OTPack<sci::NetIO> *otpack;
        TripleGenerator<sci::NetIO> *triple_gen;
        MillionaireProtocol<sci::NetIO> *mill;
        MillionaireWithEquality<sci::NetIO> *mill_eq;
        Equality<sci::NetIO> *eq;
        AuxProtocols *aux;
        int party;

        // Constructor
        GeometricPerspectiveProtocols(int party, sci::NetIO *io, sci::OTPack<sci::NetIO> *otpack);

        // Destructor
        ~GeometricPerspectiveProtocols();

        // Bit Multiplication Protocol
        void bit_mul(int32_t dim, uint64_t *input, uint64_t *output, int32_t output_bw);

        // MW(x0, x1, L) = Wrap(x0, x1, L) + MSB(x)
        void mw(int32_t dim, uint64_t *input, uint64_t *output, int32_t in_bw, int32_t out_bw);

        // Multiplexer protocol with two-bit choice, c is a two-bit number
        void mux_3(int32_t dim, uint64_t *inA, uint64_t *inC, uint64_t *out, int32_t bw);

        // new truncation protocol
        void new_truncate(int32_t dim, uint64_t *inA, uint64_t *outB, int32_t shift, int32_t bw);

        void cross_term(int32_t dim, uint64_t *inA, uint64_t *inB, uint64_t *outC, int32_t bwA, int32_t bwB, int32_t bwC);

        void signed_mul(int32_t dim, uint64_t *inA, uint64_t *inB, uint64_t *outC, int32_t bwA, int32_t bwB, int32_t bwC);

        void sirnn_unsigned_mul(int32_t dim, uint64_t *inA, uint64_t *inB, uint64_t *outC, int32_t bwA, int32_t bwB, int32_t bwC);

        void cross_term_reverse(int32_t dim, uint64_t *inA, uint64_t *inB, uint64_t *outC, int32_t bwA, int32_t bwB, int32_t bwC);

        void sign_extension(int32_t dim, uint64_t *in, uint64_t *out, int32_t in_bw, int32_t out_bw);

        void truncate_with_one_bit_error(int32_t dim, uint64_t *inA, uint64_t *outB, int32_t shift, int32_t bw);

        void msb0_truncation(int32_t dim, uint64_t *inA, uint64_t *outB, int32_t shift, int32_t bw);

        void new_msb0_truncation(int32_t dim, uint64_t *inA, uint64_t *outB, int32_t shift, int32_t bw);
};
#endif //GEOMTRIC_PERSPECTIVE_PROTOCOLS_H
