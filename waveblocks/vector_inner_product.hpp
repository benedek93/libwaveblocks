#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <tuple>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Dense>

#include "basic_types.hpp"
#include "hawp_commons.hpp"
#include "inhomogeneous_inner_product.hpp"

namespace waveblocks {

/**
 * \brief Class providing inner product calculation of multi-component
 *   wavepackets.
 *
 * \tparam D dimensionality of processed wavepackets
 * \tparam MultiIndex multi-index type of processed wavepackets
 * \tparam QR quadrature rule to use, with N nodes
 */
template<dim_t D, class MultiIndex, class QR>
class VectorInnerProduct
{
public:
    using CMatrixNN = CMatrix<Eigen::Dynamic, Eigen::Dynamic>;
    using CMatrix1N = CMatrix<1, Eigen::Dynamic>;
    using CMatrixN1 = CMatrix<Eigen::Dynamic, 1>;
    using CMatrixD1 = CMatrix<D, 1>;
    using CMatrixDD = CMatrix<D, D>;
    using CMatrixDN = CMatrix<D, Eigen::Dynamic>;
    using RMatrixD1 = RMatrix<D, 1>;
    using CDiagonalNN = Eigen::DiagonalMatrix<complex_t, Eigen::Dynamic>;
    using NodeMatrix = typename QR::NodeMatrix;
    using WeightVector = typename QR::WeightVector;
    using op_t = std::function<CMatrix1N(CMatrixDN,RMatrixD1,dim_t,dim_t)>;

    /**
     * \brief Calculate the matrix of the inner product.
     *
     * Returns the matrix elements \f$\langle \Phi | f | \Phi \rangle\f$ with
     * an operator \f$f\f$.
     * The matrix consists of \f$N \times N\f$ blocks (\f$N\f$: number of
     * components), each of size \f$|\mathfrak{K}| \times |\mathfrak{K}|\f$.
     * The coefficients of the wavepacket are ignored.
     *
     * \param[in] packet multi-component wavepacket \f$\Phi\f$
     * \param[in] op operator \f$f(x, q, i, j) : \mathbb{C}^{D \times N} \times
     *   \mathbb{R}^D \times \mathbb{N} \times \mathbb{N} \rightarrow
     *   \mathbb{C}^N\f$ which is evaluated at the
     *   nodal points \f$x\f$ and position \f$q\f$, between components 
     *   \f$i\f$ and \f$j\f$;
     *   default returns a vector of ones
     *
     * \tparam Packet packet type (e.g. HomogeneousHaWp)
     */
    template<class Packet>
    static CMatrixNN build_matrix(const Packet& packet,
                           const op_t& op=default_op) {
        const dim_t n_components = packet.n_components();

        // Calculate offsets into output matrix.
        // Needed for parallelization.
        std::vector<dim_t> offsets(n_components);
        offsets[0] = 0;
        for (dim_t i = 1; i < n_components; ++i) {
            offsets[i] = packet.component(i-1).coefficients().size();
        }

        // Allocate output matrix.
        size_t total_size = 0;
        for (auto& comp : packet.components()) {
            total_size += comp.coefficients().size();
        }
        CMatrixNN result(total_size, total_size);

        // Calculate matrix.
        using IP = InhomogeneousInnerProduct<D,MultiIndex,QR>;
        for (dim_t i = 0; i < n_components; ++i) {
            for (dim_t j = 0; j < n_components; ++j) {
                using namespace std::placeholders;
                result.block(offsets[i], offsets[j],
                        packet.component(i).coefficients().size(),
                        packet.component(j).coefficients().size()) =
                    IP::build_matrix(packet.component(i), packet.component(j),
                                     std::bind(op, _1, _2, i, j));
            }
        }

        return result;
    }

private:
    static CMatrix1N default_op(const CMatrixDN& nodes, const RMatrixD1& pos, dim_t i, dim_t j)
    {
        (void)pos;
        (void)i;
        (void)j;
        return CMatrix1N::Ones(1, nodes.cols());
    }
};

}